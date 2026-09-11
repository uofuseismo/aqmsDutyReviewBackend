#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/arrival.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/durationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationDurationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/localMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationLocalMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/alarmQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/quarry.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/database/aqms/subnetTrigger.hpp"
#include "aqmsDutyReviewBackend/database/aqms/waveform.hpp"
#include "aqmsDutyReviewBackend/database/aqms/segment.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
#include "aqmsDutyReviewBackend/hash.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;


namespace
{

/// @brief The strings the frontend sees for an event type.
/// @note Named rather than numbered: an enumerator's value is an
///       implementation detail, and a frontend switching on 3 would break
///       the day somebody inserts a type alphabetically.
[[nodiscard]] std::string toString(const Event::EventType type)
{
    switch (type)
    {
    case Event::EventType::Avalanche:     return "avalanche";
    case Event::EventType::Collapse:      return "collapse";
    case Event::EventType::Earthquake:    return "earthquake";
    case Event::EventType::Explosion:     return "explosion";
    case Event::EventType::Landslide:     return "landslide";
    case Event::EventType::MiningInduced: return "mining_induced";
    case Event::EventType::NuclearTest:   return "nuclear_test";
    case Event::EventType::QuarryBlast:   return "quarry_blast";
    case Event::EventType::Sonic:         return "sonic";
    case Event::EventType::SubnetTrigger: return "subnet_trigger";
    case Event::EventType::Unknown:       return "unknown";
    }
    return "unknown";
}

[[nodiscard]] std::string toString(const Origin::GeographicType type)
{
    switch (type)
    {
    case Origin::GeographicType::Local:       return "local";
    case Origin::GeographicType::Regional:    return "regional";
    case Origin::GeographicType::Teleseismic: return "teleseismic";
    case Origin::GeographicType::Unknown:     return "unknown";
    }
    return "unknown";
}

[[nodiscard]] std::string toString(const Origin::ReviewStatus status)
{
    switch (status)
    {
    case Origin::ReviewStatus::Automatic:  return "automatic";
    case Origin::ReviewStatus::Cancelled:  return "cancelled";
    case Origin::ReviewStatus::Human:      return "human";
    case Origin::ReviewStatus::Finalized:  return "finalized";
    case Origin::ReviewStatus::Incomplete: return "incomplete";
    }
    return "automatic";
}

[[nodiscard]] std::string toString(const IMagnitude::Type type)
{
    switch (type)
    {
    case IMagnitude::Type::Human:    return "human";
    case IMagnitude::Type::Duration: return "duration";
    case IMagnitude::Type::Local:    return "local";
    case IMagnitude::Type::Moment:   return "moment";
    }
    return "unknown";
}

/// @brief The key a magnitude is filed under on its origin.
/// @note An origin carries at most ONE magnitude of each type -
///       Origin::setMagnitudes refuses a second - so the magnitudes are an
///       object keyed by type rather than an array to be searched.  A
///       client wanting the coda measurements writes
///       origin.magnitudes.durationMagnitude.stationMagnitudes instead of
///       scanning for the entry whose magnitudeType says "duration".
[[nodiscard]] std::string toMagnitudeKey(const IMagnitude::Type type)
{
    switch (type)
    {
    case IMagnitude::Type::Human:    return "humanMagnitude";
    case IMagnitude::Type::Duration: return "durationMagnitude";
    case IMagnitude::Type::Local:    return "localMagnitude";
    case IMagnitude::Type::Moment:   return "momentMagnitude";
    }
    return "unknownMagnitude";
}

[[nodiscard]] std::string toString(const IMagnitude::ReviewStatus status)
{
    switch (status)
    {
    case IMagnitude::ReviewStatus::Automatic: return "automatic";
    case IMagnitude::ReviewStatus::Human:     return "human";
    }
    return "automatic";
}

[[nodiscard]] std::string toString(const Arrival::Phase phase)
{
    switch (phase)
    {
    case Arrival::Phase::P: return "P";
    case Arrival::Phase::S: return "S";
    }
    return "P";
}

[[nodiscard]] std::string toString(const Arrival::ReviewStatus status)
{
    switch (status)
    {
    case Arrival::ReviewStatus::Automatic: return "automatic";
    case Arrival::ReviewStatus::Human:     return "human";
    case Arrival::ReviewStatus::Finalized: return "finalized";
    }
    return "automatic";
}

/// @brief Writes a segment's samples out under the requested encoding.
/// @param[in,out] item      The segment object to write into.
/// @param[in] data          The samples.
/// @param[in] encoding      What was asked for.
///
/// Writes three keys: data, gain and deltaEncoded.  The latter two say
/// what was actually DONE rather than what was requested, so a client
/// decodes the same way every time - undo the differences if
/// deltaEncoded, then divide by gain - and that path is a no-op when
/// nothing was asked for.
void writeSamples(boost::json::object &item,
                  const std::vector<double> &data,
                  const WaveformEncoding &encoding)
{
    double gain{1};
    std::vector<double> scaled;
    const std::vector<double> *values{&data};
    if (encoding.enableQuantization && !data.empty() &&
        encoding.quantizationLevels > 0)
    {
        double maximumAbsolute{0};
        for (const auto sample : data)
        {
            maximumAbsolute = std::max(maximumAbsolute, std::abs(sample));
        }
        // A dead channel has nothing to scale.  Dividing by its maximum
        // would be a division by zero, and there is no information here to
        // lose in any case.
        if (maximumAbsolute > 0)
        {
            // The gain has to come from the data.  Samples reach here as
            // counts - order 1e4 - or as physical units - order 1e-5 - and
            // a constant that suited one would destroy the other.
            gain = encoding.quantizationLevels/maximumAbsolute;
            scaled.reserve(data.size());
            for (const auto sample : data)
            {
                scaled.push_back(std::round(sample*gain));
            }
            values = &scaled;
        }
    }

    // Differences only make sense on whole numbers.  On fractional
    // samples the client would rebuild them with a running sum, and the
    // rounding error of that sum grows with every one of the tens of
    // thousands of samples in a segment.
    const auto isWholeNumbered
        = std::all_of(values->begin(), values->end(),
                      [](const double sample)
                      {
                          return sample == std::floor(sample) &&
                                 std::abs(sample)
                                     <= 9.0e15; // exactly representable
                      });
    const auto deltaEncoded
        = encoding.enableDeltaEncoding && isWholeNumbered;

    boost::json::array samples;
    samples.reserve(values->size());
    if (deltaEncoded)
    {
        double previous{0};
        for (const auto sample : *values)
        {
            samples.push_back(static_cast<std::int64_t> (sample - previous));
            previous = sample;
        }
    }
    else if (isWholeNumbered)
    {
        // Whole numbers go out as integers even when nothing was asked
        // for.  Boost.JSON prints a double in scientific notation - 12345
        // becomes 1.2345E4 - so writing a count as a double costs
        // characters for nothing.
        for (const auto sample : *values)
        {
            samples.push_back(static_cast<std::int64_t> (sample));
        }
    }
    else
    {
        for (const auto sample : *values){samples.push_back(sample);}
    }
    item["data"] = std::move(samples);
    // Always present, both of them.  A client that reads these blindly
    // needs no branch on what it asked for, and an encoding that was
    // declined - delta on fractional samples - is visible rather than
    // silent.
    item["gain"] = gain;
    item["deltaEncoded"] = deltaEncoded;
}

/// @brief Serializes one pick.
[[nodiscard]] boost::json::object arrivalToJSON(const Arrival &arrival)
{
    boost::json::object item;
    if (arrival.hasIdentifier())
    {
        item["arrivalIdentifier"] = arrival.getIdentifier();
    }
    if (arrival.hasStreamIdentifier())
    {
        const auto streamIdentifier = arrival.getStreamIdentifier();
        if (streamIdentifier.hasNetwork())
        {
            item["network"] = streamIdentifier.getNetwork();
        }
        if (streamIdentifier.hasStation())
        {
            item["station"] = streamIdentifier.getStation();
        }
        if (streamIdentifier.hasChannel())
        {
            item["channel"] = streamIdentifier.getChannel();
        }
        // A location code is legitimately blank - "--" in SEED - so an
        // empty string here is a value and not an absence.
        if (streamIdentifier.hasLocationCode())
        {
            item["locationCode"] = streamIdentifier.getLocationCode();
        }
    }
    if (arrival.hasTime())
    {
        item["arrivalTime"] = arrival.getTime().count();
    }
    if (arrival.hasPhase())
    {
        item["phase"] = ::toString(arrival.getPhase());
    }
    if (arrival.hasReviewStatus())
    {
        item["reviewStatus"] = ::toString(arrival.getReviewStatus());
    }
    if (const auto quality = arrival.getQuality(); quality)
    {
        item["quality"] = *quality;
    }
    // The three below describe this pick's association with THIS origin
    // rather than the pick itself, so the same arrival under another
    // origin carries different ones.
    if (arrival.hasResidual())
    {
        item["residual"] = arrival.getResidual().count();
    }
    if (const auto distance = arrival.getSourceReceiverDistance(); distance)
    {
        item["sourceReceiverDistance"] = *distance;
    }
    if (const auto azimuth = arrival.getSourceReceiverAzimuth(); azimuth)
    {
        item["sourceReceiverAzimuth"] = *azimuth;
    }
    return item;
}

[[nodiscard]] std::string toString(
    const StationDurationMagnitude::ReviewStatus status)
{
    switch (status)
    {
    case StationDurationMagnitude::ReviewStatus::Automatic:
        return "automatic";
    case StationDurationMagnitude::ReviewStatus::Human:
        return "human";
    case StationDurationMagnitude::ReviewStatus::Finalized:
        return "finalized";
    }
    return "automatic";
}

[[nodiscard]] std::string toString(
    const StationLocalMagnitude::ReviewStatus status)
{
    switch (status)
    {
    case StationLocalMagnitude::ReviewStatus::Automatic:
        return "automatic";
    case StationLocalMagnitude::ReviewStatus::Human:
        return "human";
    case StationLocalMagnitude::ReviewStatus::Finalized:
        return "finalized";
    }
    return "automatic";
}

/// @brief Serializes one coda measurement behind a duration magnitude.
[[nodiscard]] boost::json::object stationDurationMagnitudeToJSON(
    const StationDurationMagnitude &stationMagnitude)
{
    boost::json::object item;
    if (stationMagnitude.hasStreamIdentifier())
    {
        // A coda belongs to a CHANNEL, so the full stream is what tells
        // two of them at one station apart.
        const auto streamIdentifier = stationMagnitude.getStreamIdentifier();
        if (streamIdentifier.hasNetwork())
        {
            item["network"] = streamIdentifier.getNetwork();
        }
        if (streamIdentifier.hasStation())
        {
            item["station"] = streamIdentifier.getStation();
        }
        if (streamIdentifier.hasChannel())
        {
            item["channel"] = streamIdentifier.getChannel();
        }
        if (streamIdentifier.hasLocationCode())
        {
            item["locationCode"] = streamIdentifier.getLocationCode();
        }
    }
    if (stationMagnitude.hasStartTime())
    {
        item["startTime"] = stationMagnitude.getStartTime().count();
    }
    if (stationMagnitude.hasMagnitude())
    {
        item["magnitude"] = stationMagnitude.getMagnitude();
    }
    if (stationMagnitude.hasReviewStatus())
    {
        // This OBSERVATION's status, not the magnitude's.  They differ -
        // an automatic coda can sit under a reviewed magnitude - so a
        // client showing "reviewed" from the magnitude alone would be
        // overstating what a person actually looked at.
        item["reviewStatus"] = ::toString(stationMagnitude.getReviewStatus());
    }
    if (stationMagnitude.hasDuration())
    {
        // coda.tau.  The measurement window runs from startTime to
        // startTime + duration, so no end time is sent - one number
        // cannot disagree with itself the way two could.
        //
        // Mind the units: startTime is NANOSECONDS and this is SECONDS,
        // so the end is startTime + duration*1e9.  Adding them directly
        // lands nine orders of magnitude out, which is far enough to be
        // obvious but only once somebody plots it.
        item["duration"] = stationMagnitude.getDuration();
    }
    if (stationMagnitude.hasWeight())
    {
        item["weight"] = stationMagnitude.getWeight();
    }
    if (stationMagnitude.hasResidual())
    {
        // AQMS stores this only on reviewed magnitudes - zero of 140,879
        // automatic coda rows carry one - so on an automatic it is the
        // reader's own subtraction of the station magnitude and the
        // network magnitude.  Not a different quantity: magres is that
        // subtraction, exactly, on every reviewed row measured.
        item["residual"] = stationMagnitude.getResidual();
    }
    item["correction"] = stationMagnitude.getCorrection();
    if (const auto distance = stationMagnitude.getSourceReceiverDistance();
        distance)
    {
        item["sourceReceiverDistance"] = *distance;
    }
    if (const auto azimuth = stationMagnitude.getSourceReceiverAzimuth();
        azimuth)
    {
        item["sourceReceiverAzimuth"] = *azimuth;
    }
    return item;
}

/// @brief Serializes one channel's contribution to a local magnitude.
/// @note One of these per CHANNEL, not per station - a local magnitude is
///       measured on two horizontals and AQMS stores a magnitude for each,
///       which on an automatic magnitude disagree.  A client rendering
///       these gets more rows than it would for codas, and that is honest
///       rather than incidental.
[[nodiscard]] boost::json::object stationLocalMagnitudeToJSON(
    const StationLocalMagnitude &stationMagnitude)
{
    boost::json::object item;
    if (stationMagnitude.hasStreamIdentifier())
    {
        const auto streamIdentifier = stationMagnitude.getStreamIdentifier();
        if (streamIdentifier.hasNetwork())
        {
            item["network"] = streamIdentifier.getNetwork();
        }
        if (streamIdentifier.hasStation())
        {
            item["station"] = streamIdentifier.getStation();
        }
        if (streamIdentifier.hasChannel())
        {
            item["channel"] = streamIdentifier.getChannel();
        }
        if (streamIdentifier.hasLocationCode())
        {
            item["locationCode"] = streamIdentifier.getLocationCode();
        }
    }
    if (stationMagnitude.hasMagnitude())
    {
        item["magnitude"] = stationMagnitude.getMagnitude();
    }
    if (stationMagnitude.hasReviewStatus())
    {
        // This OBSERVATION's status, not the magnitude's.  They differ -
        // an automatic coda can sit under a reviewed magnitude - so a
        // client showing "reviewed" from the magnitude alone would be
        // overstating what a person actually looked at.
        item["reviewStatus"] = ::toString(stationMagnitude.getReviewStatus());
    }
    if (stationMagnitude.hasResidual())
    {
        // As for a coda: stored on review, and otherwise this channel's
        // magnitude less the network magnitude - which is what a stored
        // one is, exactly.
        item["residual"] = stationMagnitude.getResidual();
    }
    if (const auto amplitude = stationMagnitude.getAmplitude(); amplitude)
    {
        // Millimetres, whatever amp.units said - the reader normalises.
        item["amplitude"] = *amplitude;
    }
    if (stationMagnitude.hasWeight())
    {
        item["weight"] = stationMagnitude.getWeight();
    }
    item["correction"] = stationMagnitude.getCorrection();
    if (const auto distance = stationMagnitude.getSourceReceiverDistance();
        distance)
    {
        item["sourceReceiverDistance"] = *distance;
    }
    if (const auto azimuth = stationMagnitude.getSourceReceiverAzimuth();
        azimuth)
    {
        item["sourceReceiverAzimuth"] = *azimuth;
    }
    return item;
}

/// @brief Serializes one magnitude.
/// @note The per-station measurements hang off the concrete type rather
///       than off IMagnitude, so reaching them means asking what this
///       actually is.  Only duration magnitudes carry them today; a local
///       magnitude's station amplitudes are the same shape and are not
///       read yet.
[[nodiscard]] boost::json::object magnitudeToJSON(const IMagnitude &magnitude)
{
    boost::json::object item;
    if (magnitude.hasIdentifier())
    {
        item["magnitudeIdentifier"] = magnitude.getIdentifier();
    }
    // Kept even though the key it is filed under says the same thing: a
    // client that iterates the object's values, or logs one entry on its
    // own, would otherwise have nothing naming the type.
    item["magnitudeType"] = ::toString(magnitude.getType());
    if (magnitude.hasValue())
    {
        item["magnitude"] = magnitude.getValue();
    }
    if (magnitude.hasReviewStatus())
    {
        item["reviewStatus"] = ::toString(magnitude.getReviewStatus());
    }
    if (magnitude.getType() == IMagnitude::Type::Local)
    {
        const auto *localMagnitude
            = dynamic_cast<const LocalMagnitude *> (&magnitude);
        if (localMagnitude != nullptr && localMagnitude->size() > 0)
        {
            boost::json::array stationMagnitudes;
            stationMagnitudes.reserve(localMagnitude->size());
            for (const auto &stationMagnitude : *localMagnitude)
            {
                stationMagnitudes.push_back(
                    ::stationLocalMagnitudeToJSON(stationMagnitude));
            }
            item["stationMagnitudes"] = std::move(stationMagnitudes);
        }
    }
    if (magnitude.getType() == IMagnitude::Type::Duration)
    {
        const auto *durationMagnitude
            = dynamic_cast<const DurationMagnitude *> (&magnitude);
        if (durationMagnitude != nullptr &&
            durationMagnitude->size() > 0)
        {
            boost::json::array stationMagnitudes;
            stationMagnitudes.reserve(durationMagnitude->size());
            for (const auto &stationMagnitude : *durationMagnitude)
            {
                stationMagnitudes.push_back(
                    ::stationDurationMagnitudeToJSON(stationMagnitude));
            }
            // Only when there are some.  An absent key says the codas were
            // not read; an empty array would say the magnitude was
            // computed from nothing.
            item["stationMagnitudes"] = std::move(stationMagnitudes);
        }
    }
    return item;
}

/// @brief Serializes one origin.
/// @param[in] withArrivals  Whether to write its picks out.  False trims
///                          them and nothing else - the origin keeps
///                          everything a client needs to list it beside
///                          the preferred solution.
[[nodiscard]] boost::json::object originToJSON(const Origin &origin,
                                               const bool withArrivals)
{
    boost::json::object item;
    if (origin.hasIdentifier())
    {
        item["originIdentifier"] = origin.getIdentifier();
    }
    // Says the same thing as the event's preferredOriginIdentifier, and is
    // here anyway: a client rendering a list of origins asks this question
    // per origin, and doing it by identifier comparison at every row is
    // work for no reason.  Unambiguous in a way the same flag on a
    // MAGNITUDE would not be - an origin is preferred or not, full stop,
    // whereas a magnitude can be its origin's preferred one without being
    // the event's, which is why magnitudes keep the identifier form.
    item["isPreferred"] = origin.isPreferred();
    if (origin.hasLatitude()){item["latitude"] = origin.getLatitude();}
    if (origin.hasLongitude()){item["longitude"] = origin.getLongitude();}
    // Meters, as the model holds it - AQMS stores kilometers and the
    // reader converts on the way in.  Absent when the solution's depth
    // never converged; origin.depth is nullable.
    if (origin.hasDepth()){item["depth"] = origin.getDepth();}
    if (origin.hasTime())
    {
        item["originTime"] = origin.getTime().count();
    }
    // The same keys the catalog uses for the same three quantities, so a
    // client reads them one way whichever endpoint it came from.
    if (const auto gap = origin.getMaximumAzimuthalGap(); gap)
    {
        item["maximumAzimuthalGap"] = *gap;
    }
    if (const auto rms = origin.getWeightedRootMeanSquaredError(); rms)
    {
        item["weightedRootMeanSquaredError"] = *rms;
    }
    if (const auto n = origin.getNumberOfDefiningPhases(); n)
    {
        item["numberOfDefiningPhases"] = *n;
    }
    if (origin.hasGeographicType())
    {
        item["geographicType"] = ::toString(origin.getGeographicType());
    }
    if (origin.hasReviewStatus())
    {
        item["reviewStatus"] = ::toString(origin.getReviewStatus());
    }
    if (const auto credit = origin.getCredit(); credit)
    {
        item["credit"] = *credit;
    }

    boost::json::object magnitudesJSON;
    if (origin.hasMagnitudes())
    {
        for (const auto &magnitude : origin.magnitudes())
        {
            if (magnitude == nullptr){continue;}
            // This origin's own preferred magnitude - origin.prefmag.  It
            // need not be the event's; see the note on toJSON(Event).
            if (magnitude->isPreferred() && magnitude->hasIdentifier())
            {
                item["preferredMagnitudeIdentifier"]
                    = magnitude->getIdentifier();
            }
            magnitudesJSON[::toMagnitudeKey(magnitude->getType())]
                = ::magnitudeToJSON(*magnitude);
        }
    }
    // An object, always present, possibly empty.  Absent keys are the
    // types this origin has no magnitude of.
    item["magnitudes"] = std::move(magnitudesJSON);

    // Always, whether or not the arrivals themselves are written.  A
    // trimmed origin still says how many picks it has, so a client can
    // show "47 picks" and decide whether to go and ask for them.
    item["arrivalCount"] = static_cast<std::int64_t> (origin.size());
    if (withArrivals)
    {
        boost::json::array arrivalsJSON;
        const auto arrivals = origin.getArrivals();
        arrivalsJSON.reserve(arrivals.size());
        for (const auto &arrival : arrivals)
        {
            arrivalsJSON.push_back(::arrivalToJSON(arrival));
        }
        // Present even when empty: an origin whose picks are not
        // associated yet is a real origin, and the frontend should iterate
        // without a special case.
        item["arrivals"] = std::move(arrivalsJSON);
    }
    // Deliberately no "arrivals": [] in the other branch.  An empty array
    // would say this origin has no picks, which is a different claim from
    // "you did not ask for them" - and the difference is a hundred picks.
    return item;
}

}

boost::json::object
AQMSDutyReviewBackend::Database::AQMS::toJSON(const Event &event,
                                              const OriginDetail detail)
{
    boost::json::object result;
    if (event.hasIdentifier())
    {
        result["eventIdentifier"] = event.getIdentifier();
    }
    if (event.hasEventType())
    {
        result["eventType"] = ::toString(event.getEventType());
    }
    result["version"] = event.getVersion();
    // Named by identifier rather than repeated as a second copy of the
    // object - the origin is already in the array below.
    if (event.hasOrigins() && event.preferredOrigin().hasIdentifier())
    {
        result["preferredOriginIdentifier"]
            = event.preferredOrigin().getIdentifier();
    }
    // event.prefmag.  Emitted even when no origin carries it: the client
    // then sees an identifier it cannot resolve, which is the truth, and
    // readEvent has already logged the oddity.
    if (event.hasPreferredMagnitudeIdentifier())
    {
        result["preferredMagnitudeIdentifier"]
            = event.getPreferredMagnitudeIdentifier();
    }

    boost::json::array originsJSON;
    if (event.hasOrigins())
    {
        const auto origins = event.origins();
        originsJSON.reserve(origins.size());
        for (const auto &origin : origins)
        {
            // The preferred origin keeps its arrivals whatever was asked
            // for - it is the solution the screen is about, and a trimmed
            // one would leave nothing to review.
            const auto withArrivals
                = (detail == OriginDetail::AllOrigins) || origin.isPreferred();
            originsJSON.push_back(::originToJSON(origin, withArrivals));
        }
    }
    result["origins"] = std::move(originsJSON);
    return result;
}

std::pair<boost::json::object, std::string>
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<EventSummary> &events)
{
    boost::json::array eventsJSON;
    eventsJSON.reserve(events.size());
    for (const auto &event : events)
    {
        boost::json::object item;
        if (event.hasIdentifier())
        {
            item["eventIdentifier"] = event.getIdentifier();
        }
        if (event.hasEventType())
        {
            item["eventType"] = ::toString(event.getEventType());
        }
        item["version"] = event.getVersion();
        if (event.hasLatitude()){item["latitude"] = event.getLatitude();}
        if (event.hasLongitude()){item["longitude"] = event.getLongitude();}
        // Meters, as the model holds it - AQMS stores kilometers and the
        // reader converts on the way in.
        if (event.hasDepth()){item["depth"] = event.getDepth();}
        if (event.hasTime())
        {
            item["originTime"] = event.getTime().count();
        }
        if (event.hasGeographicType())
        {
            item["geographicType"] = ::toString(event.getGeographicType());
        }
        if (event.hasReviewStatus())
        {
            item["reviewStatus"] = ::toString(event.getReviewStatus());
        }
        // Everything below reaches this through an outer join or a
        // nullable column, so an absent key means AQMS had nothing to say.
        if (const auto credit = event.getCredit(); credit)
        {
            item["credit"] = *credit;
        }
        if (const auto source = event.getOriginSource(); source)
        {
            item["originSource"] = *source;
        }
        if (const auto gap = event.getMaximumAzimuthalGap(); gap)
        {
            item["maximumAzimuthalGap"] = *gap;
        }
        if (const auto rms = event.getWeightedRootMeanSquaredError(); rms)
        {
            item["weightedRootMeanSquaredError"] = *rms;
        }
        if (const auto n = event.getNumberOfDefiningPhases(); n)
        {
            item["numberOfDefiningPhases"] = *n;
        }
        if (const auto magnitude = event.getMagnitudeValue(); magnitude)
        {
            item["magnitude"] = *magnitude;
        }
        if (const auto type = event.getMagnitudeType(); type)
        {
            item["magnitudeType"] = ::toString(*type);
        }
        eventsJSON.push_back(std::move(item));
    }
    boost::json::object result;
    result["events"] = std::move(eventsJSON);
    // Hash what the client will compare - the events - before the hash
    // itself is part of the object, since a hash cannot cover itself.
    auto hash = AQMSDutyReviewBackend::hash(boost::json::serialize(result));
    result["hash"] = hash;
    return std::pair {std::move(result), std::move(hash)};
}

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<EventLock> &locks)
{
    // Built as an array from the start: a default-constructed value is
    // null, and an empty result must serialize to [] so the frontend can
    // iterate it without a special case.
    boost::json::array result;
    result.reserve(locks.size());
    for (const auto &lock : locks)
    {
        boost::json::object item;
        if (lock.hasEventIdentifier())
        {
            item["eventIdentifier"] = lock.getEventIdentifier();
        }
        if (lock.hasUser())
        {
            item["user"] = lock.getUser();
        }
        // Absent rather than null when jasieventlock had no lddate - the
        // same convention the catalog uses for a column AQMS left empty.
        if (lock.hasAcquisitionTime())
        {
            item["acquiredAt"] = lock.getAcquisitionTime();
        }
        result.push_back(std::move(item));
    }
    return result;
}

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const Waveform &waveform, const WaveformEncoding &encoding)
{
    boost::json::object result;
    if (waveform.hasStreamIdentifier())
    {
        const auto streamIdentifier = waveform.getStreamIdentifier();
        if (streamIdentifier.hasNetwork())
        {
            result["network"] = streamIdentifier.getNetwork();
        }
        if (streamIdentifier.hasStation())
        {
            result["station"] = streamIdentifier.getStation();
        }
        if (streamIdentifier.hasChannel())
        {
            result["channel"] = streamIdentifier.getChannel();
        }
        // A location code is legitimately blank - "--" in SEED - so an
        // empty string here is a value and not an absence.
        if (streamIdentifier.hasLocationCode())
        {
            result["locationCode"] = streamIdentifier.getLocationCode();
        }
    }
    boost::json::array segments;
    segments.reserve(waveform.size());
    for (const auto &segment : waveform)
    {
        boost::json::object item;
        // Nanoseconds since the epoch, UTC, as the model holds it.
        item["startTime"] = segment.getStartTime().count();
        item["samplingRate"] = segment.getSamplingRate();
        ::writeSamples(item, segment.getDataReference(), encoding);
        segments.push_back(std::move(item));
    }
    result["segments"] = std::move(segments);
    return result;
}

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<Waveform> &waveforms,
    const WaveformEncoding &encoding)
{
    boost::json::array result;
    result.reserve(waveforms.size());
    for (const auto &waveform : waveforms)
    {
        result.push_back(toJSON(waveform, encoding));
    }
    return result;
}

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<SubnetTrigger> &triggers)
{
    boost::json::array result;
    result.reserve(triggers.size());
    for (const auto &trigger : triggers)
    {
        boost::json::object item;
        if (trigger.hasEventIdentifier())
        {
            item["eventIdentifier"] = trigger.getEventIdentifier();
        }
        // Nanoseconds since the epoch, UTC, as the model holds it.
        if (trigger.hasTime()){item["time"] = trigger.getTime().count();}
        if (const auto source = trigger.getOriginSource(); source)
        {
            item["originSource"] = *source;
        }
        result.push_back(std::move(item));
    }
    return result;
}

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<Station> &stations)
{
    boost::json::array result;
    result.reserve(stations.size());
    for (const auto &station : stations)
    {
        boost::json::object item;
        if (station.hasNetwork()){item["network"] = station.getNetwork();}
        if (station.hasName()){item["name"] = station.getName();}
        // Absent rather than zero: station_data allows a null position,
        // and a zero would put the station in the Gulf of Guinea.
        if (station.hasLatitude()){item["latitude"] = station.getLatitude();}
        if (station.hasLongitude())
        {
            item["longitude"] = station.getLongitude();
        }
        if (station.hasStartAndEndTime())
        {
            const auto [onDate, offDate] = station.getStartAndEndTime();
            // Seconds since the epoch, UTC, as they came out of the
            // database - the frontend formats them.
            item["onDate"] = onDate.count();
            item["offDate"] = offDate.count();
        }
        // loadTime is deliberately absent.  It is lddate, and it exists so
        // the poller can ask for only the rows that changed since it last
        // looked.  The frontend lifts the network and station off this,
        // asks where that station was when the event happened, and plots
        // it - none of which the load date bears on.
        result.push_back(std::move(item));
    }
    return result;
}

/// Every field goes out unexamined, including the action and state names.
/// AQMS runs alarms as a state machine and those strings are its
/// vocabulary; mapping them onto an enum of the states we happen to have
/// seen would turn a new state from AQMS into a parse failure here rather
/// than a word the frontend shows.
///
/// The database name is not a column - ALARM_ACTION records what ran and
/// when, never where - so it is attached by the reader and is the only
/// thing distinguishing two machines' rows once they are concatenated.
boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<AlarmAction> &alarms)
{
    boost::json::array result;
    result.reserve(alarms.size());
    for (const auto &alarm : alarms)
    {
        boost::json::object item;
        item["database"] = alarm.database;
        item["eventIdentifier"] = alarm.eventIdentifier;
        item["action"] = alarm.action;
        item["state"] = alarm.state;
        item["modificationCount"] = alarm.modificationCount;
        // Absent rather than null when AQMS left mod_time empty - the
        // same convention every other serializer here uses for a column
        // the database had nothing to say about.
        if (alarm.modificationTime)
        {
            item["modificationTime"] = alarm.modificationTime->count();
        }
        result.push_back(std::move(item));
    }
    return result;
}

/// Longitude goes out in [0, 360) because that is what Origin uses, and
/// the point of sending quarries at all is to plot them against origins
/// and ask whether an event was a blast.  Two conventions in one payload
/// would put a quarry 224 degrees from the event sitting on top of it.
///
/// The name is passed through with its case intact.  Unlike a network or
/// station code it is a label off a map rather than an identifier, and it
/// is not unique - the gazetteer carries several under a shared name.
///
/// loadTime is deliberately not emitted.  It is lddate, it exists so a
/// poller can ask for only the rows that changed since it last looked, and
/// drawing a marker does not depend on it.
boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<Quarry> &quarries)
{
    boost::json::array result;
    result.reserve(quarries.size());
    for (const auto &quarry : quarries)
    {   
        boost::json::object item;
        if (quarry.hasName()){item["name"] = quarry.getName();}
        if (quarry.hasLatitude()){item["latitude"] = quarry.getLatitude();}
        if (quarry.hasLongitude())
        {   
            item["longitude"] = quarry.getLongitude();
        }   
        if (quarry.hasStartAndEndTime())
        {   
            const auto [onDate, offDate] = quarry.getStartAndEndTime();
            // Seconds since the epoch, UTC, as they came out of the
            // database - the frontend formats them.
            item["onDate"] = onDate.count();
            item["offDate"] = offDate.count();
        }   
        result.push_back(std::move(item));
    }
    return result;
}


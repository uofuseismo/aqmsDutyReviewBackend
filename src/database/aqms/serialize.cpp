#include <string>
#include <utility>
#include <vector>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
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
AQMSDutyReviewBackend::Database::AQMS::toJSON(const Waveform &waveform)
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
        const auto &data = segment.getDataReference();
        boost::json::array samples;
        samples.reserve(data.size());
        for (const auto sample : data){samples.push_back(sample);}
        item["data"] = std::move(samples);
        segments.push_back(std::move(item));
    }
    result["segments"] = std::move(segments);
    return result;
}

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<Waveform> &waveforms)
{
    boost::json::array result;
    result.reserve(waveforms.size());
    for (const auto &waveform : waveforms)
    {
        result.push_back(toJSON(waveform));
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

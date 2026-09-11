#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SERIALIZE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SERIALIZE_HPP
#include <string>
#include <utility>
#include <vector>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Event;
 class EventLock;
 class EventSummary;
 class Quarry;
 class Station;
 class SubnetTrigger;
 class Waveform;
}

/// @file serialize.hpp
/// @brief Turns AQMS model objects into JSON on the way to the frontend.
///
/// boost::json rather than crow::json, for two reasons.  This library has
/// no business knowing a web framework exists - it would be linked into a
/// model layer purely to name a type - and the project already uses
/// boost::json for JSON web tokens, so crow::json here would mean carrying
/// two JSON libraries for one job.
///
/// Nothing is lost by the choice: a crow response body is a plain string,
/// so handing one of these to a route is
///
///     response.body = boost::json::serialize(toJSON(stations));
///
/// with no conversion in between.
///
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Serializes the locked events.
/// @result A JSON array of {eventIdentifier, user} objects.
/// @note An empty vector serializes to [] and not to null, so a frontend
///       can iterate the result without checking it first.
[[nodiscard]] boost::json::value toJSON(const std::vector<EventLock> &locks);

/// @brief Serializes the catalog and hashes it.
/// @result The pair {catalog, hash}.  The catalog is
///         {events: [...], hash: "..."} and the hash is that same hash on
///         its own, so a caller wanting only the hash need not serialize
///         the events to get one.
/// @note The hash comes back with the catalog rather than being something
///       a caller computes, because two callers computing it separately is
///       two chances to hash different bytes - and a frontend comparing a
///       hash from one endpoint with a body from another would then
///       re-download a catalog that had not changed.
/// @note The hash covers the serialized events, not the object that
///       carries it: a hash cannot cover itself.
/// @note Only the fields a summary actually has are emitted.  Nearly every
///       column behind these is nullable or comes through an outer join,
///       so a missing key means AQMS had nothing to say - not zero.
/// @note Depth is in meters and the origin time in nanoseconds since the
///       epoch, UTC, as the model holds them.
/// @note An empty vector serializes to [] and not to null.
[[nodiscard]] std::pair<boost::json::object, std::string> toJSON(
    const std::vector<EventSummary> &events);

/// @brief How much of each origin to write out.
/// @note Only affects origins that are NOT the preferred one.  The
///       preferred origin is always written in full - it is the solution
///       the screen is actually about.
enum class OriginDetail
{
    AllOrigins,          /*!< Every origin gets its arrivals. */
    PreferredOriginOnly  /*!< Only the preferred origin gets its arrivals;
                              the rest are written down to their
                              high-level fields. */
};

/// @brief Serializes one event - its origins, and each origin's
///        arrivals and magnitudes.
/// @result The event as a nested object.
/// @note Nested rather than flattened, and by IDENTIFIER rather than by
///       duplication.  Each origin, arrival and magnitude appears exactly
///       once, and the preferred ones are named by their identifiers -
///       preferredOriginIdentifier and preferredMagnitudeIdentifier on the
///       event, preferredMagnitudeIdentifier on each origin.  Repeating
///       the preferred origin at the top level would put the same object
///       in the payload twice with nothing keeping the copies equal.
/// @note Each origin also carries a plain isPreferred flag.  It is
///       redundant against the event's preferredOriginIdentifier on
///       purpose - a client rendering origins asks the question per row.
///       Magnitudes deliberately do NOT get one: see the note below.
/// @note An origin's magnitudes are an OBJECT keyed by type -
///       durationMagnitude, localMagnitude, humanMagnitude,
///       momentMagnitude - not an array.  An origin carries at most one of
///       each, so the shape says so, and a client reaches the coda
///       measurements as
///       origin.magnitudes.durationMagnitude.stationMagnitudes rather than
///       by scanning an array for a matching magnitudeType.  A type the
///       origin has no magnitude of is simply an absent key.
/// @note An origin carries preferredMagnitudeIdentifier of its own, and it
///       need not match the event's.  AQMS stores origin.prefmag and
///       event.prefmag separately, so the event's preferred magnitude can
///       belong to an origin that is not the preferred origin - which is
///       exactly why a single "isPreferred" flag could not express this.
/// @note Depth and source-receiver distance are in meters, and all times
///       are nanoseconds since the epoch, UTC, as the models hold them.
/// @note One exception, and it is a trap: a coda's \c duration is in
///       SECONDS, beside a \c startTime in nanoseconds.  Its measurement
///       window ends at startTime + duration*1e9.
///       AQMS stores both distances in kilometres; the conversion happens
///       once, in the readers.
/// @note Only the fields an object actually has are emitted; an absent key
///       means AQMS had nothing to say.
/// @note \c OriginDetail::PreferredOriginOnly trims the ARRIVALS off the
///       non-preferred origins and nothing else.  They keep their
///       position, depth, time, review status and magnitudes, so they can
///       still be listed and compared against the preferred solution -
///       what goes is the part that is actually large.  A relocation can
///       carry hundreds of picks; it carries at most a handful of
///       magnitudes.
/// @note Every origin carries \c arrivalCount whether or not its arrivals
///       were written, so a trimmed origin still says how many picks it
///       has and a client can decide whether to ask for them.  The
///       \c arrivals key is ABSENT rather than empty when it was not
///       requested: an empty array would make an origin with a hundred
///       picks look like one with none, which is the sort of difference
///       that matters here.
[[nodiscard]] boost::json::object toJSON(
    const Event &event,
    OriginDetail detail = OriginDetail::AllOrigins);

/// @brief How the samples are written onto the wire.
///
/// Both levels are off by default, which is the plain form: every sample
/// as the number it is.  Turn them on independently to trade size for
/// exactness, and read the reply's own \c gain and \c deltaEncoded back -
/// they say what was ACTUALLY done, which is not always what was asked
/// for.
struct WaveformEncoding
{
    /// @brief Send first differences instead of samples.
    /// @note Lossless, and the larger of the two savings on real data:
    ///       consecutive seismic samples are close together, so the
    ///       differences are small numbers with far fewer digits.  It is
    ///       what miniSEED's own Steim compression exploits.
    /// @note Requires whole-number samples, so it applies when the samples
    ///       are counts - which is what sampletype 'i' gives - or when
    ///       quantization has just made them whole.  Asked for on
    ///       fractional samples without quantization it is declined, and
    ///       the reply says so.
    bool enableDeltaEncoding{false};
    /// @brief Scale by a gain and round to whole numbers.
    /// @note Lossy, and the loss is bounded by \c quantizationLevels: the
    ///       gain is chosen so the largest absolute sample lands on that
    ///       many levels, so the error is at worst half a level of full
    ///       scale.
    bool enableQuantization{false};
    /// @brief How many levels the largest absolute sample is given.
    /// @note 32767 - a signed 16-bit full scale - keeps the error near
    ///       1.5e-05 of full scale.  A trace two thousand pixels tall
    ///       resolves about eleven bits, so this is already far finer than
    ///       anything a person can see, and lower values buy real size.
    /// @note Ignored unless \c enableQuantization is true.
    int quantizationLevels{32767};
};

/// @brief Serializes a waveform.
/// @result An object carrying the stream and its segments:
///         {network, station, channel, locationCode,
///          segments: [{startTime, samplingRate, data: [...]}]}
/// @note A segment gives its start time and sampling rate rather than a
///       time per sample.  The samples are evenly spaced by definition, so
///       a time array would be the same information at many times the
///       size - and these are already the heaviest thing this backend
///       sends.
/// @note Segments are in start-time order and already merged, so a break
///       between two of them is a real gap in the record rather than a
///       miniSEED packet boundary.  A client should draw them as separate
///       traces and not join them.
/// @warning This carries every sample.  A few minutes of 100 sps data is
///          tens of thousands of numbers per channel, and JSON stores each
///          as text - so a waveform response is orders of magnitude larger
///          than any other in this API.  Ask for the streams you are going
///          to draw.
/// @note Every segment carries \c gain and \c deltaEncoded whatever was
///       asked for, so one decoder handles every reply:
///       undo the differences if \c deltaEncoded, then divide by
///       \c gain.  With both levels off the gain is 1 and
///       \c deltaEncoded is false, so that same path is a no-op.
[[nodiscard]] boost::json::value toJSON(
    const Waveform &waveform,
    const WaveformEncoding &encoding = WaveformEncoding {});

/// @brief Serializes several waveforms.
/// @result A JSON array of waveform objects.
/// @note An empty vector serializes to [] and not to null.
[[nodiscard]] boost::json::value toJSON(
    const std::vector<Waveform> &waveforms,
    const WaveformEncoding &encoding = WaveformEncoding {});

/// @brief Serializes the subnet triggers.
/// @result A JSON array of {eventIdentifier, time, originSource} objects.
/// @note No position and no magnitude, because a trigger has neither.
///       What it has is a real time and the machine that raised it.
/// @note An empty vector serializes to [] and not to null.
[[nodiscard]] boost::json::value toJSON(
    const std::vector<SubnetTrigger> &triggers);

/// @brief Serializes the stations.
/// @result A JSON array of station objects.
/// @note Only the fields a station actually has are emitted - latitude and
///       longitude are nullable in station_data, so a station with no
///       position simply has no such keys rather than a zero that would
///       plot somewhere in the Gulf of Guinea.
/// @note Longitude is in [0, 360), matching what Origin uses, so the two
///       can be compared without one being negative and the other not.
/// @note The load date is not emitted.  It is bookkeeping for the poller's
///       incremental query and the frontend has no use for it: it takes
///       the network and station, asks where that station was when the
///       event happened, and plots it.
/// @note An empty vector serializes to [] and not to null.
[[nodiscard]] boost::json::value toJSON(const std::vector<Station> &stations);

/// @brief Serializes the quarries.
/// @result A JSON array of {name, latitude, longitude, onDate, offDate}
///         objects.  Longitude is in [0, 360) and the dates are seconds
///         since the epoch, UTC.
/// @note Only the fields a quarry actually has are emitted; an absent key
///       means AQMS had nothing to say, not zero.
/// @note The load date is not emitted.
/// @warning The name is not unique and not a key - several quarries share
///          one - and it keeps its case.
/// @note An empty vector serializes to [] and not to null.
[[nodiscard]] boost::json::value toJSON(const std::vector<Quarry> &quarries);

}
#endif

#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SERIALIZE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SERIALIZE_HPP
#include <string>
#include <utility>
#include <vector>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class EventLock;
 class EventSummary;
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
[[nodiscard]] boost::json::value toJSON(const Waveform &waveform);

/// @brief Serializes several waveforms.
/// @result A JSON array of waveform objects.
/// @note An empty vector serializes to [] and not to null.
[[nodiscard]] boost::json::value toJSON(
    const std::vector<Waveform> &waveforms);

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
}
#endif

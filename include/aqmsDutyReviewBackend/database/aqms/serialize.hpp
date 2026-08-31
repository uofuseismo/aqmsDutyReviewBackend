#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SERIALIZE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SERIALIZE_HPP
#include <vector>
#include <boost/json/value.hpp>

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class EventLock;
 class EventSummary;
 class Station;
 class SubnetTrigger;
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

/// @brief Serializes the catalog.
/// @result A JSON array of event summary objects.
/// @note Only the fields a summary actually has are emitted.  Nearly every
///       column behind these is nullable or comes through an outer join,
///       so a missing key means AQMS had nothing to say - not zero.
/// @note Depth is in meters and the origin time in nanoseconds since the
///       epoch, UTC, as the model holds them.
/// @note An empty vector serializes to [] and not to null.
[[nodiscard]] boost::json::value toJSON(
    const std::vector<EventSummary> &events);

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

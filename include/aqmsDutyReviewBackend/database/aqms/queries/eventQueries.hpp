#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_EVENT_QUERIES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_EVENT_QUERIES_HPP
#include <chrono>
#include <optional>
#include <memory>
#include <vector>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Event;
 class EventSummary;
 class SubnetTrigger;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief The catalog: one flattened row per event, newest first.
/// @param[in] client    A client connected to an AQMS database.
/// @param[in] duration  How far back to look from now.
/// @param[in] logger    Where a row that cannot be read is recorded.
///                      Borrowed, not owned, and must not be null.
/// @result The event summaries, most recent origin time first.
/// @note A row that cannot be read is skipped rather than failing the
///       whole catalog.  AQMS's own constraints are looser than this
///       application's in places - origin.depth permits 1000 km where the
///       model stops at 800 - so one unusual event must not cost the duty
///       analyst every other event on their screen.  Each skip is logged.
/// @note Subnet triggers are excluded - they are an Earthworm artifact
///       rather than something that happened.  See \c querySubnetTriggers
///       for those.
/// @throws std::invalid_argument if the duration is not positive.
/// @throws std::runtime_error if the query itself fails.
/// @brief Fetches one event in full - every origin, every origin's picks,
///        and every origin's magnitudes.
/// @param[in] client           The AQMS database.
/// @param[in] eventIdentifier  The event to fetch.
/// @param[in] logger           Where oddities are reported.  Borrowed.
/// @result The event, or nullopt if there is no such event.
/// @note Every origin, not just the preferred one - the point of this
///       query is comparing a relocation against what it replaced.
/// @note Two statements in one transaction: the origins and their picks,
///       then the magnitudes for those origins.  Joining magnitudes onto
///       the first would multiply every arrival row by the magnitude
///       count, and running them in separate transactions would let a
///       relocation land in between.
/// @throws std::exception if the query or the parse fails.
[[nodiscard]] std::optional<Event> queryEvent(
    const Client &client,
    int64_t eventIdentifier,
    spdlog::logger *logger);

[[nodiscard]] std::vector<EventSummary> queryEventSummaries(
    const Client &client,
    const std::chrono::seconds &duration,
    spdlog::logger *logger);

/// @brief The subnet triggers, newest first.
/// @param[in] client    A client connected to an AQMS database.
/// @param[in] duration  How far back to look from now.
/// @param[in] logger    Where a row that cannot be read is recorded.
///                      Borrowed, not owned, and must not be null.
/// @result The triggers, most recent first.
/// @note SubnetTrigger and not EventSummary.  A trigger has no location,
///       no magnitude, and nobody credited with it; carrying it as an
///       event summary would mean three placeholder columns nobody should
///       read and a dozen absent ones.  Its query is correspondingly
///       smaller - the magnitude and credit joins existed only to return
///       nulls here.
/// @note Anything in this list is unreviewed by construction: a trigger an
///       analyst decides is real is given an event type, at which point it
///       is an event and appears in the catalog instead.
/// @throws std::invalid_argument if the duration is not positive.
/// @throws std::runtime_error if the query itself fails.
[[nodiscard]] std::vector<SubnetTrigger> querySubnetTriggers(
    const Client &client,
    const std::chrono::seconds &duration,
    spdlog::logger *logger);
}
#endif

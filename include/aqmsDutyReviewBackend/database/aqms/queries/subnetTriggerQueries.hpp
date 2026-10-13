#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_SUBNET_TRIGGER_QUERIES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_SUBNET_TRIGGER_QUERIES_HPP
#include <chrono>
#include <vector>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class SubnetTrigger;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
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

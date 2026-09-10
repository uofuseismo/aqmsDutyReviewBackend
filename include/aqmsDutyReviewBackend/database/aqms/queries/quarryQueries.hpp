#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_QUARRY_QUERIES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_QUARRY_QUERIES_HPP
#include <chrono>
#include <string>
#include <vector>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Quarry;
}

/// @file quarryQueries.hpp
/// @brief Reads quarries from the AQMS Gazetteer tables.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Every quarry in the database.
/// @param[in] client  A client connected to the AQMS database.
/// @result The quarries.
/// @throws std::runtime_error if the query fails.
[[nodiscard]] std::vector<Quarry> queryQuarries(const Client &client);

}
#endif

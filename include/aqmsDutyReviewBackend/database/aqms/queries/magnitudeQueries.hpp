#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_MAGNITUDE_QUERIES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_MAGNITUDE_QUERIES_HPP
#include <cstdint>
#include <optional>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class DurationMagnitude;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Reads an origin's duration magnitude and the per-station coda
///        measurements it was computed from.
/// @param[in] client            A client connected to an AQMS database.
/// @param[in] originIdentifier  The origin whose magnitude is wanted.
/// @param[in] logger            Where a row that cannot be read is
///                              recorded.  Borrowed, may be null.
/// @result The duration magnitude, or nullopt if this origin has none -
///         which is ordinary, not an error.  Plenty of origins are located
///         and never sized this way.
///
/// @note Asks whether one exists before assembling anything.  The
///       existence check is a count on netmag alone; the magnitude itself
///       is a three-table join out to coda, and there is no reason to pay
///       for it on an origin that has no duration magnitude.
///
/// @note Both statements run in ONE transaction.  The second is keyed on a
///       magid the first returned, and across two transactions a
///       re-computed magnitude landing in between would leave the coda
///       rows of one magnitude hanging off the value of another.
///
/// @note Matches magtype 'd' only.  netmag's own CHECK also permits 'dl',
///       which this does not read - see DURATION_MAGNITUDE_EXISTS.
///
/// @throws std::exception if the query or the parse fails.
[[nodiscard]] std::optional<DurationMagnitude> queryDurationMagnitude(
    const Client &client,
    int64_t originIdentifier,
    spdlog::logger *logger = nullptr);
}
#endif

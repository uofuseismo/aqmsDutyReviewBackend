#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_DURATION_MAGNITUDE_READER_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_DURATION_MAGNITUDE_READER_HPP
#include <cstdint>
#include <vector>
#include <spdlog/logger.h>
#include <pqxx/pqxx>

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class StationDurationMagnitude;
 class StationLocalMagnitude;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Reads the codas behind one duration magnitude, inside a
///        transaction the caller already has open.
/// @param[in,out] transaction         The open transaction.
/// @param[in] magnitudeIdentifier     netmag.magid.
/// @param[in] originIdentifier        The origin the codas are associated
///                                    to - assoccoo is keyed on it, and
///                                    the same coda under another origin
///                                    carries a different distance.
/// @param[in] logger                  Where an unreadable coda is
///                                    recorded.  Borrowed, may be null.
/// @result The station magnitudes, empty if there are none.
///
/// @note A PRIVATE header - it lives under src/ and is not installed,
///       because it puts pqxx in its signature.  It exists so that the
///       event query can pull codas inside its own transaction rather than
///       opening a second one, and so that queryDurationMagnitude and the
///       event path cannot drift into reading the same rows differently.
///
/// @note Takes the magid rather than finding it.  A caller that has
///       already read netmag knows it; queryDurationMagnitude, which does
///       not, looks it up first and then calls this.
[[nodiscard]] std::vector<StationDurationMagnitude>
readStationDurationMagnitudes(pqxx::transaction_base &transaction,
                              std::int64_t magnitudeIdentifier,
                              std::int64_t originIdentifier,
                              spdlog::logger *logger);

/// @brief Reads the per-CHANNEL amplitudes behind one local magnitude,
///        inside a transaction the caller already has open.
/// @note One entry per channel, not per station.  assocamm.mag is stored
///       per component and the components disagree on automatics, so there
///       is no station value to collapse them into.
[[nodiscard]] std::vector<StationLocalMagnitude>
readStationLocalMagnitudes(pqxx::transaction_base &transaction,
                           std::int64_t magnitudeIdentifier,
                           std::int64_t originIdentifier,
                           spdlog::logger *logger);
}
#endif

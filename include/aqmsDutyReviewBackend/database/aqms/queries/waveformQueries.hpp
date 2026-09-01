#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_WAVEFORM_QUERIES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_WAVEFORM_QUERIES_HPP
#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class StreamIdentifier;
 class Waveform;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief A granular query to return a particular waveform for this event.
/// @param[in] client            A client connected to an AQMS database.
/// @param[in] eventIdentifier   The event.
/// @param[in] streamIdentifier  Which stream to fetch.
/// @result The waveform for this event and stream.
/// @note The whole waveform, merged: the time bounds are set wide, and
///       miniSEED packet boundaries are joined back together because a
///       packet boundary is not a gap in the data.
/// @throws std::runtime_error if AQMS holds no data for this event and
///         stream - an absent waveform is not an empty one, and returning
///         a Waveform with no segments would push that distinction onto
///         every caller.
[[nodiscard]] Waveform queryWaveform(const Client &client,
                                     std::int64_t eventIdentifier,
                                     const StreamIdentifier &streamIdentifier);
}
#endif

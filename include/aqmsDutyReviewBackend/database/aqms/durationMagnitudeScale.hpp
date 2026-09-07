#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DURATION_MAGNITUDE_SCALE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DURATION_MAGNITUDE_SCALE_HPP
#include "aqmsDutyReviewBackend/database/aqms/distanceCorrections.hpp"

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Computes one channel's duration magnitude from its coda length.
///
/// Utah:
/// \f[
///    M_d = -2.25 + 2.32\log_{10}(\tau) + 0.0023 r_{km} + c_{channel}
/// \f]
/// Yellowstone:
/// \f[
///    M_d = -2.60 + 2.44\log_{10}(\tau) + 0.0040 r_{km} + c_{channel}
/// \f]
///
/// @param[in] duration           The coda duration - coda.tau - in
///                               SECONDS.
/// @param[in] distance           The source-receiver distance in METERS,
///                               as the models hold it.  The formulae are
///                               written in kilometres and this converts.
/// @param[in] channelCorrection  The per-channel correction.  Zero at
///                               UUSS, and taken as an argument anyway so
///                               that a site which does use them does not
///                               have to find this function to add them.
/// @param[in] region             Which of the two scales to apply.
/// @result The station duration magnitude.
///
/// @throws std::invalid_argument if the duration is not positive.  A coda
///         of no length has no logarithm, and a station that measured no
///         coda did not measure a magnitude.
///
/// @note Unlike the local magnitude scale, this is a CLOSED FORM - there
///       is no lookup table.  The distance enters linearly rather than
///       through Richter's -log10(A0), so nothing here shares the
///       bucket-versus-nearest question that scale has.
///
/// @note The two regions differ in all three coefficients, not just the
///       constant, so they diverge with both duration and distance.  There
///       is no offset relating them and no safe default; the caller has to
///       know where the event is.
[[nodiscard]] double computeStationDurationMagnitude(
    double duration,
    double distance,
    double channelCorrection,
    MagnitudeRegion region);
}
#endif

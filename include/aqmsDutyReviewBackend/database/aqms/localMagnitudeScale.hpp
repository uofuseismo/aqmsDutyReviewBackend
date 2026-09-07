#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_LOCAL_MAGNITUDE_SCALE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_LOCAL_MAGNITUDE_SCALE_HPP
#include "aqmsDutyReviewBackend/database/aqms/distanceCorrections.hpp"

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Computes one channel's local magnitude from its Wood-Anderson
///        amplitude.
///
/// \f[
///    M_L = \log_{10}\left(\frac{A_{mm}}{2}\right)
///        + c_{static} + c_{distance}(r)
/// \f]
///
/// @param[in] amplitude         The Wood-Anderson amplitude in
///                              MILLIMETRES, which is what amp.amplitude
///                              carries for amptype 'WAS'.
/// @param[in] staticCorrection  The per-channel correction from
///                              stacorrections.
/// @param[in] distance          The source-receiver distance in METERS, as
///                              the models hold it.
/// @param[in] region            Which distance-correction table to read.
/// @result The station local magnitude.
///
/// @throws std::invalid_argument if the amplitude is not positive.  A
///         zero or negative amplitude has no logarithm, and there is no
///         defensible number to return for one - a station that measured
///         nothing did not measure a magnitude.
///
/// @note MILLIMETRES, and there is no unit conversion inside.  The
///       distance table this uses is Richter's -log10(A0) - 1.4 near zero
///       rising to 4.9 at 600 km - which is defined against a
///       Wood-Anderson amplitude in millimetres.  Reading the amplitude in
///       centimetres instead puts every magnitude exactly one unit low:
///       large enough to be obvious, small enough to look like a
///       calibration problem rather than a units bug.
///
///       Checked against four reviewed stations on magid 5513: taking the
///       stored station magnitudes and solving backwards for the distance
///       correction gives 1.398, 2.097, 2.394 and 2.765 at 7.9, 30.5, 39.7
///       and 63.2 km - which sit on the Utah table.  Under a centimetre
///       reading every one of those would be 1.0 higher and match nothing.
///
/// @note amp.units is NOT always 'mm'.  Most Wood-Anderson rows are, but
///       the archive also holds them in 'cm', so a caller reading the amp
///       table has to convert rather than assume.
///
/// @note The division by two takes a PEAK-TO-PEAK amplitude to a
///       zero-to-peak one.  It is part of the scale as UUSS applies it,
///       not a unit conversion, so an amplitude that is already
///       zero-to-peak must not be passed here.
///
/// @note The distance correction is looked up here rather than passed in,
///       so a caller cannot pair an amplitude with the wrong region's
///       table by accident.
[[nodiscard]] double computeStationLocalMagnitude(
    double amplitude,
    double staticCorrection,
    double distance,
    MagnitudeRegion region);
}
#endif

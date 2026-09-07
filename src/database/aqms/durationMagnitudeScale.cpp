#include <cmath>
#include <stdexcept>
#include "aqmsDutyReviewBackend/database/aqms/durationMagnitudeScale.hpp"

namespace
{
/// The scale coefficients, as UUSS writes them:
///     Md = constant + slope*log10(tau) + distanceTerm*r_km + correction
struct Coefficients
{
    double constant{0};
    double slope{0};
    double distanceTerm{0};
};
constexpr Coefficients utah{-2.25, 2.32, 0.0023};
constexpr Coefficients yellowstone{-2.60, 2.44, 0.0040};
}

double AQMSDutyReviewBackend::Database::AQMS::computeStationDurationMagnitude(
    const double duration,
    const double distance,
    const double channelCorrection,
    const MagnitudeRegion region)
{
    if (!(duration > 0))
    {
        // Also catches a NaN, which would otherwise propagate into a
        // magnitude that compares false against everything.
        throw std::invalid_argument("Coda duration must be positive");
    }
    const auto &coefficients
        = (region == MagnitudeRegion::Yellowstone) ? ::yellowstone : ::utah;
    // The formulae are written in kilometres; the models hold meters.
    constexpr double metersPerKilometer{1000};
    const auto distanceInKilometers = distance/metersPerKilometer;
    return coefficients.constant
         + coefficients.slope*std::log10(duration)
         + coefficients.distanceTerm*distanceInKilometers
         + channelCorrection;
}

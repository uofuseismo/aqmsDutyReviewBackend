#include <cmath>
#include <stdexcept>
#include "aqmsDutyReviewBackend/database/aqms/localMagnitudeScale.hpp"
#include "aqmsDutyReviewBackend/database/aqms/distanceCorrections.hpp"

double AQMSDutyReviewBackend::Database::AQMS::computeStationLocalMagnitude(
    const double amplitude,
    const double staticCorrection,
    const double distance,
    const MagnitudeRegion region)
{
    if (!(amplitude > 0))
    {
        // Also catches a NaN, which would otherwise propagate silently
        // into a magnitude that compares false against everything.
        throw std::invalid_argument("Wood-Anderson amplitude must be "
                                    "positive");
    }
    // Peak-to-peak to zero-to-peak.  No unit conversion: the amplitude is
    // used in millimetres, as the scale is defined.
    constexpr double peakToPeak{2};
    return std::log10(amplitude/peakToPeak)
         + staticCorrection
         + getDistanceCorrection(distance, region);
}

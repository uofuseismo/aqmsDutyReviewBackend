#include <memory>
#include <stdexcept>
#include <optional>
#include <utility>
#include <GeographicLib/Geodesic.hpp>
#include <GeographicLib/Constants.hpp>
#include "aqmsDutyReviewBackend/database/aqms/geodesic.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
using namespace AQMSDutyReviewBackend::Database::AQMS::Geodesic;

class DistanceAzimuth::DistanceAzimuthImpl
{
public:
    double mDistance{0};
    double mAzimuth{0};
    double mBackAzimuth{0};
    bool mHaveDistance{false};
    bool mHaveAzimuth{false};
    bool mHaveBackAzimuth{false};
};

/// Constructor
DistanceAzimuth::DistanceAzimuth() :
    pImpl(std::make_unique<DistanceAzimuthImpl> ())
{
}

/// Copy constructor
DistanceAzimuth::DistanceAzimuth(const DistanceAzimuth &distaz)
{
    *this = distaz;
}

/// Move constructor
DistanceAzimuth::DistanceAzimuth(DistanceAzimuth &&distaz) noexcept
{
    *this = std::move(distaz);
}

/// Constructor
DistanceAzimuth::DistanceAzimuth(const Origin &origin, const Station &station) :
    pImpl(std::make_unique<DistanceAzimuthImpl> ())
{
    // These throw
    auto sourceLatitude = origin.getLatitude();
    auto sourceLongitude = origin.getLongitude();
    auto receiverLatitude = station.getLatitude();
    auto receiverLongitude = station.getLongitude();
    const GeographicLib::Geodesic geodesic{GeographicLib::Constants::WGS84_a(),
                                           GeographicLib::Constants::WGS84_f()};
    double azimuth;
    double backAzimuth;
    double distance;
    auto greatCircleDistance
        = geodesic.Inverse(sourceLatitude, sourceLongitude,
                           receiverLatitude, receiverLongitude,
                           distance, azimuth, backAzimuth);
    // Translate azimuth from [-180,180] to [0,360].
    if (azimuth < 0){azimuth = azimuth + 360;}
    // Translate azimuth from [-180,180] to [0,360] then convert to a
    // back-azimuth by subtracting 180, i.e., +180.
    backAzimuth = backAzimuth + 180;
    // Lock it in
    setDistance(distance*1.e-3);
    setAzimuth(azimuth);
    setBackAzimuth(backAzimuth);
}

/// Copy assignment
DistanceAzimuth& DistanceAzimuth::operator=(const DistanceAzimuth &distaz)
{
    if (&distaz == this){return *this;}
    pImpl = std::make_unique<DistanceAzimuthImpl> (*distaz.pImpl);
    return *this;
}

/// Move assignment
DistanceAzimuth& DistanceAzimuth::operator=(DistanceAzimuth &&distaz) noexcept
{
    if (&distaz == this){return *this;}
    pImpl = std::move(distaz.pImpl);
    return *this;
}

/// Destructor
DistanceAzimuth::~DistanceAzimuth() = default;

/// Distance
void DistanceAzimuth::setDistance(const double distance)
{
    if (distance < 0)
    {
        throw std::invalid_argument("Distance must be non-negative");
    }
    pImpl->mDistance = distance;
    pImpl->mHaveDistance = true;
}

std::optional<double> DistanceAzimuth::getDistance() const noexcept
{
    return pImpl->mHaveDistance ?
           std::make_optional<double> (pImpl->mDistance) : std::nullopt;
}

/// Azimuth
void DistanceAzimuth::setAzimuth(const double azimuth)
{
    if (azimuth < 0 || azimuth >= 360)
    {
        throw std::invalid_argument("Azimuth must be in range [0, 360)");
    }
    pImpl->mAzimuth = azimuth;
    pImpl->mHaveAzimuth = true;
} 

std::optional<double> DistanceAzimuth::getAzimuth() const noexcept
{
    return pImpl->mHaveAzimuth ?
           std::make_optional<double> (pImpl->mAzimuth) : std::nullopt;
}

/// Back azimuth
void DistanceAzimuth::setBackAzimuth(const double backAzimuth)
{
    if (backAzimuth < 0 || backAzimuth >= 360)
    {
        throw std::invalid_argument("Back azimuth must be in range [0, 360]");
    }
    pImpl->mBackAzimuth = backAzimuth;
    pImpl->mHaveBackAzimuth = true;
}

std::optional<double> DistanceAzimuth::getBackAzimuth() const noexcept
{
    return pImpl->mHaveBackAzimuth ?
           std::make_optional<double> (pImpl->mBackAzimuth) : std::nullopt;
}


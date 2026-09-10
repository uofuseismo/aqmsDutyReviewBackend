#include <cctype>
#include <chrono>
#include <cmath>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <boost/algorithm/string/trim.hpp>
#include "aqmsDutyReviewBackend/database/aqms/quarry.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

constexpr double minimumLatitude{-90};
constexpr double maximumLatitude{90};

}

class Quarry::QuarryImpl
{
public:
    std::string mNetwork;
    std::string mName;
    std::pair<std::chrono::seconds, std::chrono::seconds> mStartAndEndTime;
    std::chrono::seconds mLoadTime{0};
    double mLatitude{0};
    double mLongitude{0};
    bool mHasName{false};
    bool mHasStartAndEndTime{false};
    bool mHasLoadTime{false};
    bool mHasLatitude{false};
    bool mHasLongitude{false};
};

/// Constructor
Quarry::Quarry() :
    pImpl(std::make_unique<QuarryImpl> ())
{
}

/// Copy constructor
Quarry::Quarry(const Quarry &station)
{
    *this = station;
}

/// Move constructor
Quarry::Quarry(Quarry &&station) noexcept
{
    *this = std::move(station);
}

/// Copy assignment
Quarry& Quarry::operator=(const Quarry &station)
{
    if (&station == this){return *this;}
    pImpl = std::make_unique<QuarryImpl> (*station.pImpl);
    return *this;
}

/// Move assignment
Quarry& Quarry::operator=(Quarry &&station) noexcept
{
    if (&station == this){return *this;}
    pImpl = std::move(station.pImpl);
    return *this;
}

/// Destructor
Quarry::~Quarry() = default;

/// Name
void Quarry::setName(const std::string &name)
{
    auto normalized = name;
    boost::algorithm::trim(normalized);
    pImpl->mName = std::move(normalized);
    pImpl->mHasName = true;
}

std::string Quarry::getName() const
{
    if (!hasName()){throw std::runtime_error("Quarry name not set");}
    return pImpl->mName;
}

bool Quarry::hasName() const noexcept
{
    return pImpl->mHasName;
}

/// Latitude
void Quarry::setLatitude(const double latitude)
{
    if (latitude < minimumLatitude || latitude > maximumLatitude)
    {
        throw std::invalid_argument("Latitude must be in range ["
                                  + std::to_string(minimumLatitude) + ","
                                  + std::to_string(maximumLatitude) + "]");
    }
    pImpl->mLatitude = latitude;
    pImpl->mHasLatitude = true;
}

double Quarry::getLatitude() const
{
    if (!hasLatitude()){throw std::runtime_error("Latitude not set");}
    return pImpl->mLatitude;
}

bool Quarry::hasLatitude() const noexcept
{
    return pImpl->mHasLatitude;
}

/// Longitude
void Quarry::setLongitude(const double longitude) noexcept
{
    // Normalized to [0, 360) the same way Origin does it, so a station and
    // an origin can be compared without one of them being -111.89 and the
    // other 248.11.
    auto lon = std::fmod(longitude, 360.0);
    if (lon < 0){lon = lon + 360.0;}
    pImpl->mLongitude = lon;
    pImpl->mHasLongitude = true;
}

double Quarry::getLongitude() const
{
    if (!hasLongitude()){throw std::runtime_error("Longitude not set");}
    return pImpl->mLongitude;
}

bool Quarry::hasLongitude() const noexcept
{
    return pImpl->mHasLongitude;
}

/// On and off dates
void Quarry::setStartAndEndTime(
    const std::pair<std::chrono::seconds, std::chrono::seconds> &startAndEndTime)
{
    if (startAndEndTime.first > startAndEndTime.second)
    {
        throw std::invalid_argument(
            "Quarry start time cannot be after its end time");
    }
    pImpl->mStartAndEndTime = startAndEndTime;
    pImpl->mHasStartAndEndTime = true;
}

std::pair<std::chrono::seconds, std::chrono::seconds>
Quarry::getStartAndEndTime() const
{
    if (!hasStartAndEndTime())
    {
        throw std::runtime_error("Start and end time not set");
    }
    return pImpl->mStartAndEndTime;
}

bool Quarry::hasStartAndEndTime() const noexcept
{
    return pImpl->mHasStartAndEndTime;
}

/// Load date
void Quarry::setLoadTime(const std::chrono::seconds &loadTime) noexcept
{
    pImpl->mLoadTime = loadTime;
    pImpl->mHasLoadTime = true;
}

std::chrono::seconds Quarry::getLoadTime() const
{
    if (!hasLoadTime()){throw std::runtime_error("Load time not set");}
    return pImpl->mLoadTime;
}

bool Quarry::hasLoadTime() const noexcept
{
    return pImpl->mHasLoadTime;
}

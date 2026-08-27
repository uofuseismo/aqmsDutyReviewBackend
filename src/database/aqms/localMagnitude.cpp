#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include "aqmsDutyReviewBackend/database/aqms/localMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationLocalMagnitude.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

class LocalMagnitude::LocalMagnitudeImpl
{
public:
    std::vector<StationLocalMagnitude> mStationLocalMagnitudes;
};

/// Constructor
LocalMagnitude::LocalMagnitude() :
    IMagnitude(),
    pImpl(std::make_unique<LocalMagnitudeImpl> ())
{
}

/// Copy constructor
LocalMagnitude::LocalMagnitude(const LocalMagnitude &magnitude) :
    IMagnitude(magnitude),
    pImpl(std::make_unique<LocalMagnitudeImpl> (*magnitude.pImpl))
{
}

/// Move constructor
LocalMagnitude::LocalMagnitude(LocalMagnitude &&magnitude) noexcept
{
    *this = std::move(magnitude);
}

/// Copy assignment
LocalMagnitude& LocalMagnitude::operator=(const LocalMagnitude &magnitude)
{
    if (&magnitude == this){return *this;}
    IMagnitude::operator=(magnitude);
    pImpl = std::make_unique<LocalMagnitudeImpl> (*magnitude.pImpl);
    return *this;
}

/// Move assignment
LocalMagnitude& LocalMagnitude::operator=(LocalMagnitude &&magnitude) noexcept
{
    if (&magnitude == this){return *this;}
    pImpl = std::move(magnitude.pImpl);
    IMagnitude::operator=(std::move(magnitude));
    return *this;
}

/// Destructor
LocalMagnitude::~LocalMagnitude() = default;

/// Type
IMagnitude::Type LocalMagnitude::getType() const noexcept
{
    return IMagnitude::Type::Local;
}

/// Clone
std::unique_ptr<IMagnitude> LocalMagnitude::clone() const
{
    return std::make_unique<LocalMagnitude> (*this);
}

/// Station magnitudes
void LocalMagnitude::setStationMagnitudes(
    const std::vector<StationLocalMagnitude> &stationMagnitudes)
{
    pImpl->mStationLocalMagnitudes = stationMagnitudes;
}

void LocalMagnitude::setStationMagnitudes(
    std::vector<StationLocalMagnitude> &&stationMagnitudes)
{
    pImpl->mStationLocalMagnitudes = std::move(stationMagnitudes);
}

std::vector<StationLocalMagnitude>
LocalMagnitude::getStationMagnitudes() const
{
    return pImpl->mStationLocalMagnitudes;
}

size_t LocalMagnitude::size() const noexcept
{
    return pImpl->mStationLocalMagnitudes.size();
}

/// Iterators
LocalMagnitude::const_iterator LocalMagnitude::begin() const
{
    return pImpl->mStationLocalMagnitudes.begin();
}

LocalMagnitude::const_iterator LocalMagnitude::cbegin() const
{
    return pImpl->mStationLocalMagnitudes.cbegin();
}

LocalMagnitude::const_iterator LocalMagnitude::end() const
{
    return pImpl->mStationLocalMagnitudes.end();
}

LocalMagnitude::const_iterator LocalMagnitude::cend() const
{
    return pImpl->mStationLocalMagnitudes.cend();
}

const StationLocalMagnitude& LocalMagnitude::at(size_t pos) const
{
    return pImpl->mStationLocalMagnitudes.at(pos);
}

const StationLocalMagnitude& LocalMagnitude::operator[](size_t pos) const
{
    return pImpl->mStationLocalMagnitudes[pos];
}


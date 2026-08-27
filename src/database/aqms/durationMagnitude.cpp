#include <cstddef>
#include <memory>
#include <utility>
#include <vector>
#include "aqmsDutyReviewBackend/database/aqms/durationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationDurationMagnitude.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

class DurationMagnitude::DurationMagnitudeImpl
{
public:
    std::vector<StationDurationMagnitude> mStationDurationMagnitudes;
};

/// Constructor
DurationMagnitude::DurationMagnitude() :
    IMagnitude(),
    pImpl(std::make_unique<DurationMagnitudeImpl> ())
{
}

/// Copy constructor
DurationMagnitude::DurationMagnitude(const DurationMagnitude &magnitude) :
    IMagnitude(magnitude),
    pImpl(std::make_unique<DurationMagnitudeImpl> (*magnitude.pImpl))
{
}

/// Move constructor
DurationMagnitude::DurationMagnitude(DurationMagnitude &&magnitude) noexcept
{
    *this = std::move(magnitude);
}

/// Copy assignment
DurationMagnitude&
DurationMagnitude::operator=(const DurationMagnitude &magnitude)
{
    if (&magnitude == this){return *this;}
    IMagnitude::operator=(magnitude);
    pImpl = std::make_unique<DurationMagnitudeImpl> (*magnitude.pImpl);
    return *this;
}

/// Move assignment
DurationMagnitude&
DurationMagnitude::operator=(DurationMagnitude &&magnitude) noexcept
{
    if (&magnitude == this){return *this;}
    pImpl = std::move(magnitude.pImpl);
    IMagnitude::operator=(std::move(magnitude));
    return *this;
}

/// Destructor
DurationMagnitude::~DurationMagnitude() = default;

/// Type
IMagnitude::Type DurationMagnitude::getType() const noexcept
{
    return IMagnitude::Type::Duration;
}

/// Clone
std::unique_ptr<IMagnitude> DurationMagnitude::clone() const
{
    return std::make_unique<DurationMagnitude> (*this);
}

/// Station magnitudes
void DurationMagnitude::setStationMagnitudes(
    const std::vector<StationDurationMagnitude> &stationMagnitudes)
{
    pImpl->mStationDurationMagnitudes = stationMagnitudes;
}

void DurationMagnitude::setStationMagnitudes(
    std::vector<StationDurationMagnitude> &&stationMagnitudes)
{
    pImpl->mStationDurationMagnitudes = std::move(stationMagnitudes);
}

std::vector<StationDurationMagnitude>
DurationMagnitude::getStationMagnitudes() const
{
    return pImpl->mStationDurationMagnitudes;
}

size_t DurationMagnitude::size() const noexcept
{
    return pImpl->mStationDurationMagnitudes.size();
}

/// Iterators
DurationMagnitude::const_iterator DurationMagnitude::begin() const
{
    return pImpl->mStationDurationMagnitudes.begin();
}

DurationMagnitude::const_iterator DurationMagnitude::cbegin() const
{
    return pImpl->mStationDurationMagnitudes.cbegin();
}

DurationMagnitude::const_iterator DurationMagnitude::end() const
{
    return pImpl->mStationDurationMagnitudes.end();
}

DurationMagnitude::const_iterator DurationMagnitude::cend() const
{
    return pImpl->mStationDurationMagnitudes.cend();
}

const StationDurationMagnitude& DurationMagnitude::at(size_t pos) const
{
    return pImpl->mStationDurationMagnitudes.at(pos);
}

const StationDurationMagnitude& DurationMagnitude::operator[](size_t pos) const
{
    return pImpl->mStationDurationMagnitudes[pos];
}


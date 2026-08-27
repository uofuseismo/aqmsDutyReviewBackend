#include <memory>
#include <utility>
#include "aqmsDutyReviewBackend/database/aqms/centroidMomentTensorMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

class CentroidMomentTensorMagnitude::CentroidMomentTensorMagnitudeImpl
{
};

/// Constructor
CentroidMomentTensorMagnitude::CentroidMomentTensorMagnitude() :
    IMagnitude(),
    pImpl(std::make_unique<CentroidMomentTensorMagnitudeImpl> ())
{
}

/// Copy constructor
CentroidMomentTensorMagnitude::CentroidMomentTensorMagnitude(
    const CentroidMomentTensorMagnitude &magnitude) :
    IMagnitude(magnitude),
    pImpl(std::make_unique<CentroidMomentTensorMagnitudeImpl>(*magnitude.pImpl))
{
}

/// Move constructor
CentroidMomentTensorMagnitude::CentroidMomentTensorMagnitude(
    CentroidMomentTensorMagnitude &&magnitude) noexcept
{
    *this = std::move(magnitude);
}

/// Copy assignment
CentroidMomentTensorMagnitude& 
CentroidMomentTensorMagnitude::operator=(
    const CentroidMomentTensorMagnitude &magnitude)
{
    if (&magnitude == this){return *this;}
    IMagnitude::operator=(magnitude);
    pImpl = std::make_unique<CentroidMomentTensorMagnitudeImpl> (*magnitude.pImpl);
    return *this;
}

/// Move assignment
CentroidMomentTensorMagnitude& 
CentroidMomentTensorMagnitude::operator=(
    CentroidMomentTensorMagnitude &&magnitude) noexcept
{
    if (&magnitude == this){return *this;}
    pImpl = std::move(magnitude.pImpl);
    IMagnitude::operator=(std::move(magnitude));
    return *this;
}

/// Destructor
CentroidMomentTensorMagnitude::~CentroidMomentTensorMagnitude() = default;

/// Type
IMagnitude::Type CentroidMomentTensorMagnitude::getType() const noexcept
{
    return IMagnitude::Type::Moment;
}

/// Clone
std::unique_ptr<IMagnitude> CentroidMomentTensorMagnitude::clone() const
{
    return std::make_unique<CentroidMomentTensorMagnitude> (*this);
}

/// Review status - a human-assigned magnitude is human-reviewed by definition.
IMagnitude::ReviewStatus CentroidMomentTensorMagnitude::getReviewStatus() const noexcept
{
    return IMagnitude::ReviewStatus::Human;
}

/// Always set: the status is fixed by the type, so a caller guarding on
/// hasReviewStatus() must not be told there is nothing to read.
bool CentroidMomentTensorMagnitude::hasReviewStatus() const noexcept
{
    return true;
}

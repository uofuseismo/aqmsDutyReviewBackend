#include <chrono>
#include <optional>
#include <memory>
#include <stdexcept>
#include <utility>
#include "aqmsDutyReviewBackend/database/aqms/stationDurationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

class StationDurationMagnitude::StationDurationMagnitudeImpl
{
public:
    StreamIdentifier mStreamIdentifier;
    std::optional<double> mSourceReceiverDistance;
    std::optional<double> mSourceReceiverAzimuth;
    std::chrono::nanoseconds mStartTime{0};
    double mMagnitude{0};
    double mDuration{0};
    double mCorrection{0};
    double mResidual{0};
    double mWeight{0};
    bool mHasMagnitude{false};
    bool mHasDuration{false};
    bool mHasResidual{false};
    bool mHasWeight{false};
    bool mHasStartTime{false};
    bool mHasStreamIdentifier{false};
};

/// Constructor
StationDurationMagnitude::StationDurationMagnitude() :
    pImpl(std::make_unique<StationDurationMagnitudeImpl> ())
{
}

/// Copy constructor
StationDurationMagnitude::StationDurationMagnitude(
    const StationDurationMagnitude &magnitude)
{
    *this = magnitude;
}

/// Move constructor
StationDurationMagnitude::StationDurationMagnitude(
    StationDurationMagnitude &&magnitude) noexcept
{
    *this = std::move(magnitude);
}

/// Copy assignment
StationDurationMagnitude&
StationDurationMagnitude::operator=(const StationDurationMagnitude &magnitude)
{
    if (&magnitude == this){return *this;}
    pImpl = std::make_unique<StationDurationMagnitudeImpl> (*magnitude.pImpl);
    return *this;
}

/// Move assignment
StationDurationMagnitude&
StationDurationMagnitude::operator=(
    StationDurationMagnitude &&magnitude) noexcept
{
    if (&magnitude == this){return *this;}
    pImpl = std::move(magnitude.pImpl);
    return *this;
}

/// Destructor
StationDurationMagnitude::~StationDurationMagnitude() = default;

/// Duration
void StationDurationMagnitude::setDuration(const double duration)
{
    if (duration <= 0)
    {
        throw std::invalid_argument("Duration must be positive");
    }
    pImpl->mDuration = duration;
    pImpl->mHasDuration = true;
}

double StationDurationMagnitude::getDuration() const
{
    if (!hasDuration()){throw std::runtime_error("Duration not set");}
    return pImpl->mDuration;
}

bool StationDurationMagnitude::hasDuration() const noexcept
{
    return pImpl->mHasDuration;
}

/// Distance

/// Correction
void StationDurationMagnitude::setCorrection(const double correction) noexcept
{
    pImpl->mCorrection = correction;
}

double StationDurationMagnitude::getCorrection() const noexcept
{
    return pImpl->mCorrection;
}

/// Residual
void StationDurationMagnitude::setResidual(const double residual) noexcept
{
    pImpl->mResidual = residual;
    pImpl->mHasResidual = true;
}

double StationDurationMagnitude::getResidual() const
{
    if (!hasResidual()){throw std::runtime_error("Residual not set");}
    return pImpl->mResidual;
}

bool StationDurationMagnitude::hasResidual() const noexcept
{
    return pImpl->mHasResidual;
}

/// Weight
void StationDurationMagnitude::setWeight(const double weight)
{
    if (weight < 0 || weight > 1)
    {
        throw std::invalid_argument("Weight must be in the range [0, 1]");
    }
    pImpl->mWeight = weight;
    pImpl->mHasWeight = true;
}

double StationDurationMagnitude::getWeight() const
{
    if (!hasWeight()){throw std::runtime_error("Weight not set");}
    return pImpl->mWeight;
}

bool StationDurationMagnitude::hasWeight() const noexcept
{
    return pImpl->mHasWeight;
}

/// Measurement window start
void StationDurationMagnitude::setStartTime(
    const std::chrono::nanoseconds &startTime) noexcept
{
    pImpl->mStartTime = startTime;
    pImpl->mHasStartTime = true;
}

std::chrono::nanoseconds StationDurationMagnitude::getStartTime() const
{
    if (!hasStartTime()){throw std::runtime_error("Start time not set");}
    return pImpl->mStartTime;
}

bool StationDurationMagnitude::hasStartTime() const noexcept
{
    return pImpl->mHasStartTime;
}

/// Source-receiver distance - meters, as everywhere else
void StationDurationMagnitude::setSourceReceiverDistance(
    const double distance)
{
    if (distance < 0)
    {
        throw std::invalid_argument("Source-receiver distance cannot be "
                                    "negative");
    }
    pImpl->mSourceReceiverDistance = distance;
}

std::optional<double>
StationDurationMagnitude::getSourceReceiverDistance() const noexcept
{
    return pImpl->mSourceReceiverDistance;
}

/// Source-receiver azimuth
void StationDurationMagnitude::setSourceReceiverAzimuth(const double azimuth)
{
    // Closed at both ends - 0 and 360 name the same direction and there is
    // no reason to reject a row for writing the other one.
    if (azimuth < 0 || azimuth > 360)
    {
        throw std::invalid_argument(
            "Source-receiver azimuth must be in range [0,360]");
    }
    pImpl->mSourceReceiverAzimuth = azimuth;
}

std::optional<double>
StationDurationMagnitude::getSourceReceiverAzimuth() const noexcept
{
    return pImpl->mSourceReceiverAzimuth;
}

/// Stream identifier
void StationDurationMagnitude::setStreamIdentifier(
    const StreamIdentifier &identifier)
{
    auto copy = identifier;
    setStreamIdentifier(std::move(copy));
}

void StationDurationMagnitude::setStreamIdentifier(
    StreamIdentifier &&identifier)
{
    // The same four parts Arrival insists on.  A station magnitude belongs
    // to a channel, and a partial stream cannot name one.
    if (!identifier.hasNetwork())
    {
        throw std::invalid_argument("Network not set on stream identifier");
    }
    if (!identifier.hasStation())
    {
        throw std::invalid_argument("Station not set on stream identifier");
    }
    if (!identifier.hasChannel())
    {
        throw std::invalid_argument("Channel not set on stream identifier");
    }
    if (!identifier.hasLocationCode())
    {
        throw std::invalid_argument(
            "Location code not set on stream identifier");
    }
    pImpl->mStreamIdentifier = std::move(identifier);
    pImpl->mHasStreamIdentifier = true;
}

StreamIdentifier StationDurationMagnitude::getStreamIdentifier() const
{
    if (!hasStreamIdentifier())
    {
        throw std::runtime_error("Stream identifier not set");
    }
    return pImpl->mStreamIdentifier;
}

bool StationDurationMagnitude::hasStreamIdentifier() const noexcept
{
    return pImpl->mHasStreamIdentifier;
}

/// Station magnitude - assoccom.mag
void StationDurationMagnitude::setMagnitude(const double magnitude) noexcept
{
    pImpl->mMagnitude = magnitude;
    pImpl->mHasMagnitude = true;
}

double StationDurationMagnitude::getMagnitude() const
{
    if (!hasMagnitude()){throw std::runtime_error("Magnitude not set");}
    return pImpl->mMagnitude;
}

bool StationDurationMagnitude::hasMagnitude() const noexcept
{
    return pImpl->mHasMagnitude;
}

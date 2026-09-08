#include <chrono>
#include <optional>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include "aqmsDutyReviewBackend/database/aqms/stationLocalMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/peakToPeakAmplitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{
/// @brief A stream identifier's four codes as a comparable tuple; an unset
///        code is treated as an empty string.
[[nodiscard]] std::string streamKey(const StreamIdentifier &identifier)
{
    const std::string network{identifier.hasNetwork() ?
                             identifier.getNetwork() : ""};
    const std::string station{identifier.hasStation() ?
                             identifier.getStation() : ""};
    const std::string channel{identifier.hasChannel() ?
                             identifier.getChannel() : ""};
    const std::string locationCode{identifier.hasLocationCode() ?
                                  identifier.getLocationCode() : ""};
    return network + "." + station + "." + channel + "." + locationCode;
}
}

class StationLocalMagnitude::StationLocalMagnitudeImpl
{
public:
    StationLocalMagnitude::ReviewStatus mReviewStatus{StationLocalMagnitude::ReviewStatus::Automatic};
    bool mHasReviewStatus{false};
        StreamIdentifier mStreamIdentifier;
    std::optional<double> mAmplitude;
    std::optional<double> mSourceReceiverDistance;
    std::optional<double> mSourceReceiverAzimuth;
    double mMagnitude{0};
    double mResidual{0};
    double mCorrection{0};
    bool mHasMagnitude{false};
    bool mHasResidual{false};
    bool mHasStreamIdentifier{false};
    double mWeight{0};
    bool mHasWeight{false};
};

/// Constructor
StationLocalMagnitude::StationLocalMagnitude() :
    pImpl(std::make_unique<StationLocalMagnitudeImpl> ())
{
}

/// Copy constructor
StationLocalMagnitude::StationLocalMagnitude(
    const StationLocalMagnitude &magnitude)
{
    *this = magnitude;
}

/// Move constructor
StationLocalMagnitude::StationLocalMagnitude(
    StationLocalMagnitude &&magnitude) noexcept
{
    *this = std::move(magnitude);
}

/// Copy assignment
StationLocalMagnitude&
StationLocalMagnitude::operator=(const StationLocalMagnitude &magnitude)
{
    if (&magnitude == this){return *this;}
    pImpl = std::make_unique<StationLocalMagnitudeImpl> (*magnitude.pImpl);
    return *this;
}

/// Move assignment
StationLocalMagnitude&
StationLocalMagnitude::operator=(StationLocalMagnitude &&magnitude) noexcept
{
    if (&magnitude == this){return *this;}
    pImpl = std::move(magnitude.pImpl);
    return *this;
}

/// Destructor
StationLocalMagnitude::~StationLocalMagnitude() = default;



/// Weight
void StationLocalMagnitude::setWeight(const double weight)
{
    if (weight < 0 || weight > 1)
    {
        throw std::invalid_argument("Weight must be in the range [0, 1]");
    }
    pImpl->mWeight = weight;
    pImpl->mHasWeight = true;
}

double StationLocalMagnitude::getWeight() const
{
    if (!hasWeight()){throw std::runtime_error("Weight not set");}
    return pImpl->mWeight;
}

bool StationLocalMagnitude::hasWeight() const noexcept
{
    return pImpl->mHasWeight;
}

/// Channel magnitude - assocamm.mag
void StationLocalMagnitude::setMagnitude(const double magnitude) noexcept
{
    pImpl->mMagnitude = magnitude;
    pImpl->mHasMagnitude = true;
}

double StationLocalMagnitude::getMagnitude() const
{
    if (!hasMagnitude()){throw std::runtime_error("Magnitude not set");}
    return pImpl->mMagnitude;
}

bool StationLocalMagnitude::hasMagnitude() const noexcept
{
    return pImpl->mHasMagnitude;
}

/// Residual
void StationLocalMagnitude::setResidual(const double residual) noexcept
{
    pImpl->mResidual = residual;
    pImpl->mHasResidual = true;
}

double StationLocalMagnitude::getResidual() const
{
    if (!hasResidual()){throw std::runtime_error("Residual not set");}
    return pImpl->mResidual;
}

bool StationLocalMagnitude::hasResidual() const noexcept
{
    return pImpl->mHasResidual;
}

/// Correction
void StationLocalMagnitude::setCorrection(const double correction) noexcept
{
    pImpl->mCorrection = correction;
}

double StationLocalMagnitude::getCorrection() const noexcept
{
    return pImpl->mCorrection;
}

/// Wood-Anderson amplitude - millimetres
void StationLocalMagnitude::setAmplitude(const double amplitude)
{
    if (!(amplitude > 0))
    {
        throw std::invalid_argument("Amplitude must be positive");
    }
    pImpl->mAmplitude = amplitude;
}

std::optional<double> StationLocalMagnitude::getAmplitude() const noexcept
{
    return pImpl->mAmplitude;
}

/// Source-receiver distance - meters
void StationLocalMagnitude::setSourceReceiverDistance(const double distance)
{
    if (distance < 0)
    {
        throw std::invalid_argument("Source-receiver distance cannot be "
                                    "negative");
    }
    pImpl->mSourceReceiverDistance = distance;
}

std::optional<double>
StationLocalMagnitude::getSourceReceiverDistance() const noexcept
{
    return pImpl->mSourceReceiverDistance;
}

/// Source-receiver azimuth
void StationLocalMagnitude::setSourceReceiverAzimuth(const double azimuth)
{
    if (azimuth < 0 || azimuth > 360)
    {
        throw std::invalid_argument(
            "Source-receiver azimuth must be in range [0,360]");
    }
    pImpl->mSourceReceiverAzimuth = azimuth;
}

std::optional<double>
StationLocalMagnitude::getSourceReceiverAzimuth() const noexcept
{
    return pImpl->mSourceReceiverAzimuth;
}

/// Stream identifier
void StationLocalMagnitude::setStreamIdentifier(
    const StreamIdentifier &identifier)
{
    auto copy = identifier;
    setStreamIdentifier(std::move(copy));
}

void StationLocalMagnitude::setStreamIdentifier(StreamIdentifier &&identifier)
{
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

StreamIdentifier StationLocalMagnitude::getStreamIdentifier() const
{
    if (!hasStreamIdentifier())
    {
        throw std::runtime_error("Stream identifier not set");
    }
    return pImpl->mStreamIdentifier;
}

bool StationLocalMagnitude::hasStreamIdentifier() const noexcept
{
    return pImpl->mHasStreamIdentifier;
}

/// Review status of this observation
void StationLocalMagnitude::setReviewStatus(const ReviewStatus status) noexcept
{
    pImpl->mReviewStatus = status;
    pImpl->mHasReviewStatus = true;
}

StationLocalMagnitude::ReviewStatus StationLocalMagnitude::getReviewStatus() const
{
    if (!hasReviewStatus())
    {
        throw std::runtime_error("Review status not set");
    }
    return pImpl->mReviewStatus;
}

bool StationLocalMagnitude::hasReviewStatus() const noexcept
{
    return pImpl->mHasReviewStatus;
}

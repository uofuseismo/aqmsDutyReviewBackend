#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

/// The same bounds Origin and IMagnitude enforce.  A summary is a flattened
/// event, so a value it rejects that they accept - or the reverse - would
/// mean the catalog and the event detail disagreed about what is possible.
constexpr double minimumLatitude{-90};
constexpr double maximumLatitude{90};
// Exactly AQMS's ORIGIN04 constraint - depth between -10 and 1000 km -
// converted to meters.  Neither end is reachable by a real earthquake, but
// a value the database is willing to store must be a value this can read,
// or one odd row costs the analyst their whole catalog.
constexpr double minimumDepth{-10000};
constexpr double maximumDepth{1000000};
constexpr double maximumMagnitude{11};
constexpr double minimumAzimuthalGap{0};
constexpr double maximumAzimuthalGap{360};

}

class EventSummary::EventSummaryImpl
{
public:
    std::optional<std::string> mCredit;
    std::optional<std::string> mOriginSource;
    std::optional<double> mMaximumAzimuthalGap;
    std::optional<double> mWeightedRootMeanSquaredError;
    std::optional<double> mMagnitudeValue;
    std::optional<int> mNumberOfDefiningPhases;
    std::optional<IMagnitude::Type> mMagnitudeType;
    std::chrono::nanoseconds mTime{0};
    int64_t mIdentifier{0};
    int mVersion{0};
    double mLatitude{0};
    double mLongitude{0};
    double mDepth{0};
    Event::EventType mEventType{Event::EventType::Unknown};
    Origin::GeographicType mGeographicType{Origin::GeographicType::Local};
    Origin::ReviewStatus mReviewStatus{Origin::ReviewStatus::Automatic};
    bool mHasIdentifier{false};
    bool mHasEventType{false};
    bool mHasLatitude{false};
    bool mHasLongitude{false};
    bool mHasDepth{false};
    bool mHasTime{false};
    bool mHasGeographicType{false};
    bool mHasReviewStatus{false};
};

/// Constructor
EventSummary::EventSummary() :
    pImpl(std::make_unique<EventSummaryImpl> ())
{
}

/// Copy constructor
EventSummary::EventSummary(const EventSummary &event)
{
    *this = event;
}

/// Move constructor
EventSummary::EventSummary(EventSummary &&event) noexcept
{
    *this = std::move(event);
}

/// Copy assignment
EventSummary& EventSummary::operator=(const EventSummary &event)
{
    if (&event == this){return *this;}
    // The whole impl, not a member list: everything here is a value type,
    // so the compiler's copy cannot forget a field the way a hand-written
    // one can.
    pImpl = std::make_unique<EventSummaryImpl> (*event.pImpl);
    return *this;
}

/// Move assignment
EventSummary& EventSummary::operator=(EventSummary &&event) noexcept
{
    if (&event == this){return *this;}
    pImpl = std::move(event.pImpl);
    return *this;
}

/// Destructor
EventSummary::~EventSummary() = default;

/// Identifier
void EventSummary::setIdentifier(const int64_t identifier)
{
    pImpl->mIdentifier = identifier;
    pImpl->mHasIdentifier = true;
}

int64_t EventSummary::getIdentifier() const
{
    if (!hasIdentifier())
    {
        throw std::runtime_error("Event identifier not set");
    }
    return pImpl->mIdentifier;
}

bool EventSummary::hasIdentifier() const noexcept
{
    return pImpl->mHasIdentifier;
}

/// Event type
void EventSummary::setEventType(const Event::EventType type) noexcept
{
    pImpl->mEventType = type;
    pImpl->mHasEventType = true;
}

Event::EventType EventSummary::getEventType() const
{
    if (!hasEventType()){throw std::runtime_error("Event type not set");}
    return pImpl->mEventType;
}

bool EventSummary::hasEventType() const noexcept
{
    return pImpl->mHasEventType;
}

/// Version
void EventSummary::setVersion(const int version)
{
    // AQMS counts versions up from zero as an event is revised, so a
    // negative one is not an older version - it is a mistake.
    if (version < 0)
    {
        throw std::invalid_argument("Version cannot be negative");
    }
    pImpl->mVersion = version;
}

int EventSummary::getVersion() const noexcept
{
    return pImpl->mVersion;
}

/// Latitude
void EventSummary::setLatitude(const double latitude)
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

double EventSummary::getLatitude() const
{
    if (!hasLatitude()){throw std::runtime_error("Latitude not set");}
    return pImpl->mLatitude;
}

bool EventSummary::hasLatitude() const noexcept
{
    return pImpl->mHasLatitude;
}

/// Longitude
void EventSummary::setLongitude(const double longitude)
{
    // Normalized to [0, 360) the same way Origin and Station do it, so a
    // summary and the event it summarizes cannot disagree about where the
    // same point is.
    auto lon = std::fmod(longitude, 360.0);
    if (lon < 0){lon = lon + 360.0;}
    pImpl->mLongitude = lon;
    pImpl->mHasLongitude = true;
}

double EventSummary::getLongitude() const
{
    if (!hasLongitude()){throw std::runtime_error("Longitude not set");}
    return pImpl->mLongitude;
}

bool EventSummary::hasLongitude() const noexcept
{
    return pImpl->mHasLongitude;
}

/// Depth
void EventSummary::setDepth(const double depth)
{
    if (depth < minimumDepth || depth > maximumDepth)
    {
        throw std::invalid_argument("Depth must be in range ["
                                  + std::to_string(minimumDepth) + ","
                                  + std::to_string(maximumDepth) + "] meters");
    }
    pImpl->mDepth = depth;
    pImpl->mHasDepth = true;
}

double EventSummary::getDepth() const
{
    if (!hasDepth()){throw std::runtime_error("Depth not set");}
    return pImpl->mDepth;
}

bool EventSummary::hasDepth() const noexcept
{
    return pImpl->mHasDepth;
}

/// Origin time
void EventSummary::setTime(const std::chrono::nanoseconds &time)
{
    pImpl->mTime = time;
    pImpl->mHasTime = true;
}

std::chrono::nanoseconds EventSummary::getTime() const
{
    if (!hasTime()){throw std::runtime_error("Origin time not set");}
    return pImpl->mTime;
}

bool EventSummary::hasTime() const noexcept
{
    return pImpl->mHasTime;
}

/// Credit
void EventSummary::setCredit(const std::string &credit)
{
    // A blank credit is no credit - the subsource column this comes from is
    // legitimately empty for an automatic location, and an empty string
    // would name nobody while claiming somebody.
    constexpr std::string_view whitespace{" \t\r\n"};
    const auto begin = credit.find_first_not_of(whitespace);
    if (begin == std::string::npos)
    {
        pImpl->mCredit = std::nullopt;
        return;
    }
    const auto end = credit.find_last_not_of(whitespace);
    pImpl->mCredit
        = std::make_optional<std::string> (credit.substr(begin,
                                                         end - begin + 1));
}

std::optional<std::string> EventSummary::getCredit() const noexcept
{
    return pImpl->mCredit;
}

/// Origin source
void EventSummary::setOriginSource(const std::string &source)
{
    // Same treatment as the credit, and for the same reason: the column is
    // nullable, and an empty string would name a producer while naming
    // nothing.
    constexpr std::string_view whitespace{" \t\r\n"};
    const auto begin = source.find_first_not_of(whitespace);
    if (begin == std::string::npos)
    {
        pImpl->mOriginSource = std::nullopt;
        return;
    }
    const auto end = source.find_last_not_of(whitespace);
    pImpl->mOriginSource
        = std::make_optional<std::string> (source.substr(begin,
                                                         end - begin + 1));
}

std::optional<std::string> EventSummary::getOriginSource() const noexcept
{
    return pImpl->mOriginSource;
}

/// Azimuthal gap
void EventSummary::setMaximumAzimuthalGap(const double gap)
{
    // Closed at both ends.  A single station has no second azimuth to
    // close the gap with, so its gap really is the whole circle - 360 is a
    // legitimate value and not an off-by-one.
    if (gap < minimumAzimuthalGap || gap > maximumAzimuthalGap)
    {
        throw std::invalid_argument("Maximum azimuthal gap must be in range ["
                                  + std::to_string(minimumAzimuthalGap) + ","
                                  + std::to_string(maximumAzimuthalGap) + "]");
    }
    pImpl->mMaximumAzimuthalGap = std::make_optional<double> (gap);
}

std::optional<double> EventSummary::getMaximumAzimuthalGap() const
{
    return pImpl->mMaximumAzimuthalGap;
}

/// Weighted RMS error
void EventSummary::setWeightedRootMeanSquaredError(const double wrmse)
{
    if (wrmse < 0)
    {
        throw std::invalid_argument(
            "Weighted root mean squared error cannot be negative");
    }
    pImpl->mWeightedRootMeanSquaredError = std::make_optional<double> (wrmse);
}

std::optional<double>
EventSummary::getWeightedRootMeanSquaredError() const
{
    return pImpl->mWeightedRootMeanSquaredError;
}

/// Defining phases
void EventSummary::setNumberOfDefiningPhases(const int nDefiningPhases)
{
    if (nDefiningPhases <= 0)
    {
        throw std::invalid_argument(
            "Number of defining phases must be positive");
    }
    pImpl->mNumberOfDefiningPhases = std::make_optional<int> (nDefiningPhases);
}

std::optional<int> EventSummary::getNumberOfDefiningPhases() const noexcept
{
    return pImpl->mNumberOfDefiningPhases;
}

/// Geographic type
void EventSummary::setGeographicType(
    const Origin::GeographicType type) noexcept
{
    pImpl->mGeographicType = type;
    pImpl->mHasGeographicType = true;
}

Origin::GeographicType EventSummary::getGeographicType() const
{
    if (!hasGeographicType())
    {
        throw std::runtime_error("Geographic type not set");
    }
    return pImpl->mGeographicType;
}

bool EventSummary::hasGeographicType() const noexcept
{
    return pImpl->mHasGeographicType;
}

/// Review status
void EventSummary::setReviewStatus(const Origin::ReviewStatus status) noexcept
{
    pImpl->mReviewStatus = status;
    pImpl->mHasReviewStatus = true;
}

Origin::ReviewStatus EventSummary::getReviewStatus() const
{
    if (!hasReviewStatus()){throw std::runtime_error("Review status not set");}
    return pImpl->mReviewStatus;
}

bool EventSummary::hasReviewStatus() const noexcept
{
    return pImpl->mHasReviewStatus;
}

/// Preferred magnitude value
void EventSummary::setMagnitudeValue(const double magnitude)
{
    // The same ceiling IMagnitude enforces.  A summary that rejected a
    // magnitude the magnitude class accepts would make the catalog and the
    // event detail disagree about what is representable.
    if (magnitude > maximumMagnitude)
    {
        throw std::invalid_argument("Magnitude cannot exceed "
                                  + std::to_string(maximumMagnitude));
    }
    pImpl->mMagnitudeValue = std::make_optional<double> (magnitude);
}

std::optional<double> EventSummary::getMagnitudeValue() const noexcept
{
    return pImpl->mMagnitudeValue;
}

/// Preferred magnitude type
void EventSummary::setMagnitudeType(
    const IMagnitude::Type magnitudeType) noexcept
{
    pImpl->mMagnitudeType = std::make_optional<IMagnitude::Type> (magnitudeType);
}

std::optional<IMagnitude::Type> EventSummary::getMagnitudeType() const noexcept
{
    return pImpl->mMagnitudeType;
}

#include <algorithm>
#include <cstdint>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

/// @brief Deep-copies a vector of polymorphic magnitudes via clone().
std::vector<std::unique_ptr<IMagnitude>>
cloneMagnitudes(const std::vector<std::unique_ptr<IMagnitude>> &magnitudes)
{
    std::vector<std::unique_ptr<IMagnitude>> result;
    result.reserve(magnitudes.size());
    for (const auto &magnitude : magnitudes)
    {
        result.push_back(magnitude ? magnitude->clone() : nullptr);
    }
    return result;
}

}

class Event::EventImpl
{
public:
    EventImpl() = default;
    EventImpl(const EventImpl &impl){*this = impl;}
    EventImpl(EventImpl &&impl) noexcept = default;
    /// The origins hold magnitudes by unique_ptr, so Origin's own copy
    /// does the cloning; this only has to name every member.
    /// @warning Every member is named here by hand.  A member added above
    ///          and not added here is silently dropped by every copy - so
    ///          adding one means editing this too.
    EventImpl& operator=(const EventImpl &impl)
    {
        if (&impl == this){return *this;}
        mOrigins = impl.mOrigins;
        mIdentifier = impl.mIdentifier;
        mPreferredMagnitudeIdentifier = impl.mPreferredMagnitudeIdentifier;
        mVersion = impl.mVersion;
        mEventType = impl.mEventType;
        mHasIdentifier = impl.mHasIdentifier;
        mHasPreferredMagnitudeIdentifier
            = impl.mHasPreferredMagnitudeIdentifier;
        mHasEventType = impl.mHasEventType;
        return *this;
    }
    EventImpl& operator=(EventImpl &&impl) noexcept = default;
    ~EventImpl() = default;

    std::vector<Origin> mOrigins;
    int64_t mIdentifier{0};
    int64_t mPreferredMagnitudeIdentifier{0};
    int mVersion{0};
    EventType mEventType{EventType::Unknown};
    bool mHasIdentifier{false};
    bool mHasPreferredMagnitudeIdentifier{false};
    bool mHasEventType{false};
};

/// Constructor
Event::Event() :
    pImpl(std::make_unique<EventImpl> ())
{
}

/// Copy constructor
Event::Event(const Event &event)
{
    *this = event;
}

/// Move constructor
Event::Event(Event &&event) noexcept
{
    *this = std::move(event);
}

/// Copy assignment
Event& Event::operator=(const Event &event)
{
    if (&event == this){return *this;}
    pImpl = std::make_unique<EventImpl> (*event.pImpl);
    return *this;
}

/// Move assignment
Event& Event::operator=(Event &&event) noexcept
{
    if (&event == this){return *this;}
    pImpl = std::move(event.pImpl);
    return *this;
}

/// Destructor
Event::~Event() = default;

/// Identifier
void Event::setIdentifier(const int64_t identifier)
{
    pImpl->mIdentifier = identifier;
    pImpl->mHasIdentifier = true;
}

int64_t Event::getIdentifier() const
{
    if (!hasIdentifier()){throw std::runtime_error("Event identifier not set");}
    return pImpl->mIdentifier;
}

bool Event::hasIdentifier() const noexcept
{
    return pImpl->mHasIdentifier;
}

/// Origins
void Event::setOrigins(const std::vector<Origin> &origins)
{
    auto copy = origins;
    setOrigins(std::move(copy));
}

void Event::setOrigins(std::vector<Origin> &&origins)
{
    if (origins.empty())
    {
        throw std::invalid_argument("There must be at least one origin");
    }
    std::set<int64_t> originIdentifiers;
    for (const auto &origin : origins)
    {
        if (!origin.hasIdentifier())
        { 
            throw std::invalid_argument("An origin is missing its identifier");
        }
        if (!origin.hasLatitude())
        {
            throw std::invalid_argument("An origin is missing its latitude");
        }
        if (!origin.hasLongitude())
        {
            throw std::invalid_argument("An origin is missing its longitude");
        }
        // Depth is deliberately NOT required.  origin.depth is nullable in
        // AQMS - it is the one locating column that is - and a solution
        // with a free depth that did not converge simply has none.  A
        // depthless origin is still an origin: it has a position and a
        // time, which is what the review screen plots.  Insisting on one
        // here made a single such origin throw away every OTHER origin on
        // the event, and the event with them.
        if (!origin.hasTime())
        {
            throw std::invalid_argument("An origin is missing its time");
        }
        auto identifier = origin.getIdentifier();
        if (originIdentifiers.contains(identifier))
        {
            throw std::invalid_argument("Duplicate origin identifier "
                                      + std::to_string(identifier));
        }
        else
        {
            originIdentifiers.insert(identifier);
        }
    }
    auto nPreferred = std::count_if(origins.begin(), origins.end(),
                                    [](const Origin &origin)
                                    {
                                        return origin.isPreferred();
                                    });
    if (nPreferred != 1)
    {
        throw std::invalid_argument(
            "There must be exactly one preferred origin - received "
           + std::to_string(nPreferred) 
           + " preferred out of "
           + std::to_string(origins.size())
           + " origins provided");
    }
    pImpl->mOrigins = origins;
}

bool Event::hasOrigins() const noexcept
{
    return !pImpl->mOrigins.empty();
}

Origin Event::getPreferredOrigin() const
{
    if (!hasOrigins()){throw std::runtime_error("Origins not set");}
    for (const auto &origin : pImpl->mOrigins)
    {
        if (origin.isPreferred()){return origin;}
    }
    // setOrigins guarantees a preferred origin exists, so this is unreachable
    // in practice.
    throw std::runtime_error("No preferred origin");
}

std::vector<Origin> Event::getOrigins() const
{
    if (!hasOrigins()){throw std::runtime_error("Origins not set");}
    return pImpl->mOrigins;
}

/// Zero-copy views
std::span<const Origin> Event::origins() const &
{
    if (!hasOrigins()){throw std::runtime_error("Origins not set");}
    return pImpl->mOrigins;
}

const Origin &Event::preferredOrigin() const &
{
    if (!hasOrigins()){throw std::runtime_error("Origins not set");}
    for (const auto &origin : pImpl->mOrigins)
    {
        if (origin.isPreferred()){return origin;}
    }
    // setOrigins guarantees a preferred origin exists, so this is
    // unreachable in practice.
    throw std::runtime_error("No preferred origin");
}

namespace
{
/// @brief Finds the event's preferred magnitude and the origin holding it.
/// @note One walk, used by preferredMagnitude and
///       preferredMagnitudeOrigin, so the two cannot disagree about which
///       magnitude is meant.
/// @note event.prefmag names a magnitude that lives on an origin, so this
///       is a search and not a member read - and it can fail in a way that
///       is worth naming precisely, because "no preferred magnitude" and
///       "prefmag points at a magnitude this event does not carry" send
///       somebody looking in very different places.
[[nodiscard]] std::pair<const AQMSDutyReviewBackend::Database::AQMS::Origin *,
                        const AQMSDutyReviewBackend::Database::AQMS::IMagnitude *>
findPreferredMagnitude(const AQMSDutyReviewBackend::Database::AQMS::Event &event)
{
    if (!event.hasPreferredMagnitudeIdentifier())
    {
        throw std::runtime_error("Preferred magnitude identifier not set");
    }
    if (!event.hasOrigins())
    {
        throw std::runtime_error("Origins not set");
    }
    const auto identifier = event.getPreferredMagnitudeIdentifier();
    for (const auto &origin : event.origins())
    {
        if (!origin.hasMagnitudes()){continue;}
        for (const auto &magnitude : origin.magnitudes())
        {
            if (magnitude->hasIdentifier() &&
                magnitude->getIdentifier() == identifier)
            {
                return {&origin, magnitude.get()};
            }
        }
    }
    throw std::runtime_error(
        "No origin on this event holds magnitude "
      + std::to_string(identifier) + " - event.prefmag names a magnitude "
        "that was not read with the origins");
}
}

const IMagnitude &Event::preferredMagnitude() const &
{
    return *findPreferredMagnitude(*this).second;
}

const Origin &Event::preferredMagnitudeOrigin() const &
{
    return *findPreferredMagnitude(*this).first;
}

std::unique_ptr<IMagnitude> Event::getPreferredMagnitude() const
{
    return preferredMagnitude().clone();
}

bool Event::hasPreferredMagnitude() const noexcept
{
    if (!hasPreferredMagnitudeIdentifier() || !hasOrigins()){return false;}
    const auto identifier = pImpl->mPreferredMagnitudeIdentifier;
    for (const auto &origin : pImpl->mOrigins)
    {
        if (!origin.hasMagnitudes()){continue;}
        for (const auto &magnitude : origin.magnitudes())
        {
            if (magnitude->hasIdentifier() &&
                magnitude->getIdentifier() == identifier)
            {
                return true;
            }
        }
    }
    return false;
}

/// Preferred magnitude identifier - event.prefmag
void Event::setPreferredMagnitudeIdentifier(const int64_t identifier)
{
    pImpl->mPreferredMagnitudeIdentifier = identifier;
    pImpl->mHasPreferredMagnitudeIdentifier = true;
}

int64_t Event::getPreferredMagnitudeIdentifier() const
{
    if (!hasPreferredMagnitudeIdentifier())
    {
        throw std::runtime_error("Preferred magnitude identifier not set");
    }
    return pImpl->mPreferredMagnitudeIdentifier;
}

bool Event::hasPreferredMagnitudeIdentifier() const noexcept
{
    return pImpl->mHasPreferredMagnitudeIdentifier;
}

/// Version
void Event::setVersion(const int version)
{
    // AQMS counts versions up from zero as an event is revised, so a
    // negative one is not an older version - it is a mistake.
    if (version < 0)
    {
        throw std::invalid_argument("Version cannot be negative");
    }
    pImpl->mVersion = version;
}

int Event::getVersion() const noexcept
{
    return pImpl->mVersion;
}

/// Event type
void Event::setEventType(const EventType type) noexcept
{
    pImpl->mEventType = type;
    pImpl->mHasEventType = true;
}

Event::EventType Event::getEventType() const
{
    if (!hasEventType()){throw std::runtime_error("Event type not set");}
    return pImpl->mEventType;
}

bool Event::hasEventType() const noexcept
{
    return pImpl->mHasEventType;
}

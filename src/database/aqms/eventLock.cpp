#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

class EventLock::EventLockImpl
{
public:
    std::string mUser;
    std::string mAcquisitionTime;
    int64_t mEventIdentifier{0};
    bool mHasEventIdentifier{false};
    bool mHasUser{false};
    bool mHasAcquisitionTime{false};
};

/// Constructor
EventLock::EventLock() :
    pImpl(std::make_unique<EventLockImpl> ())
{
}

/// Copy constructor
EventLock::EventLock(const EventLock &lock)
{
    *this = lock;
}

/// Move constructor
EventLock::EventLock(EventLock &&lock) noexcept
{
    *this = std::move(lock);
}

/// Copy assignment
EventLock& EventLock::operator=(const EventLock &lock)
{
    if (&lock == this){return *this;}
    pImpl = std::make_unique<EventLockImpl> (*lock.pImpl);
    return *this;
}

/// Move assignment
EventLock& EventLock::operator=(EventLock &&lock) noexcept
{
    if (&lock == this){return *this;}
    pImpl = std::move(lock.pImpl);
    return *this;
}

/// Destructor
EventLock::~EventLock() = default;

/// Event identifier
void EventLock::setEventIdentifier(const int64_t identifier) noexcept
{
    pImpl->mEventIdentifier = identifier;
    pImpl->mHasEventIdentifier = true;
}

int64_t EventLock::getEventIdentifier() const
{
    if (!hasEventIdentifier())
    {
        throw std::runtime_error("Event identifier not set");
    }
    return pImpl->mEventIdentifier;
}

bool EventLock::hasEventIdentifier() const noexcept
{
    return pImpl->mHasEventIdentifier;
}

/// User
void EventLock::setUser(const std::string &user)
{
    if (user.empty())
    {
        // A lock held by nobody reads as no lock at all, which is the one
        // wrong answer that could put two analysts on the same event.
        throw std::invalid_argument("User is empty");
    }
    pImpl->mUser = user;
    pImpl->mHasUser = true;
}

std::string EventLock::getUser() const
{
    if (!hasUser()){throw std::runtime_error("User not set");}
    return pImpl->mUser;
}

bool EventLock::hasUser() const noexcept
{
    return pImpl->mHasUser;
}

/// Acquisition time
void EventLock::setAcquisitionTime(const std::string &acquisitionTime)
{
    if (acquisitionTime.empty())
    {
        throw std::invalid_argument("Acquisition time is empty");
    }
    pImpl->mAcquisitionTime = acquisitionTime;
    pImpl->mHasAcquisitionTime = true;
}

std::string EventLock::getAcquisitionTime() const
{
    if (!hasAcquisitionTime())
    {
        throw std::runtime_error("Acquisition time not set");
    }
    return pImpl->mAcquisitionTime;
}

bool EventLock::hasAcquisitionTime() const noexcept
{
    return pImpl->mHasAcquisitionTime;
}

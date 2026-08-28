#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_EVENT_LOCK_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_EVENT_LOCK_HPP
#include <cstdint>
#include <memory>
#include <string>

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class EventLock eventLock.hpp
/// @brief An event somebody is currently working on.  This corresponds to
///        a row of jasieventlock.
/// @note There is at most one of these per event - jasieventlock's primary
///       key is the event identifier - so a lock has exactly one holder
///       and there is never a conflict to resolve.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class EventLock
{
public:
    /// @brief Constructor.
    EventLock();
    /// @brief Copy constructor.
    EventLock(const EventLock &lock);
    /// @brief Move constructor.
    EventLock(EventLock &&lock) noexcept;

    /// @brief Sets the identifier of the locked event.
    void setEventIdentifier(int64_t identifier) noexcept;
    /// @result The identifier of the locked event.
    /// @throws std::runtime_error if \c hasEventIdentifier() is false.
    [[nodiscard]] int64_t getEventIdentifier() const;
    /// @result True indicates the event identifier was set.
    [[nodiscard]] bool hasEventIdentifier() const noexcept;

    /// @brief Sets who holds the lock.
    /// @param[in] user  The user name.
    /// @note jasieventlock.username is nullable, so AQMS can hold a lock
    ///       for nobody in particular.  Whoever reads that row decides
    ///       what to call such a holder; this refuses to call them
    ///       nothing, because a lock with a blank holder reads as an
    ///       unlocked event.
    /// @throws std::invalid_argument if the user is empty.
    void setUser(const std::string &user);
    /// @result Who holds the lock.
    /// @throws std::runtime_error if \c hasUser() is false.
    [[nodiscard]] std::string getUser() const;
    /// @result True indicates the user was set.
    [[nodiscard]] bool hasUser() const noexcept;

    /// @brief Destructor.
    ~EventLock();
    /// @brief Copy assignment.
    EventLock& operator=(const EventLock &lock);
    /// @brief Move assignment.
    EventLock& operator=(EventLock &&lock) noexcept;
private:
    class EventLockImpl;
    std::unique_ptr<EventLockImpl> pImpl;
};
}
#endif

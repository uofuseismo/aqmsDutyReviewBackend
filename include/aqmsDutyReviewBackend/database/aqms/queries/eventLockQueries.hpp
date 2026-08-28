#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_EVENT_LOCK_QUERIES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_EVENT_LOCK_QUERIES_HPP
#include <vector>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class EventLock;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Every event currently locked, and who holds it.
/// @param[in] client  A client connected to an AQMS database.
/// @result The locks, ordered by event identifier.  Empty when nothing is
///         locked.
/// @note One row per event - jasieventlock's primary key is evid - so
///       there is nothing to deduplicate and an event appears at most
///       once.
/// @note The whole table comes back, because it is small: locks are held
///       by the handful of analysts on duty, so this is five or ten rows
///       and filtering server-side would cost more than it saved.  The
///       frontend intersects the list with the events it is showing.
/// @note A lock whose user name is NULL - the column allows it - comes
///       back as "unknown" rather than being dropped.  It is still a lock,
///       and reporting the event as free because nobody wrote a name down
///       would be worse than reporting it as held by somebody unnamed.
/// @throws std::runtime_error if the query fails.
[[nodiscard]] std::vector<EventLock> queryEventLocks(const Client &client);
}
#endif

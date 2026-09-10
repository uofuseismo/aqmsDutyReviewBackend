#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_ACTIONS_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_ACTIONS_HPP
#include <cstdint>
#include <memory>
#include <vector>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Marks an event as one a human has reviewed and stands behind.
/// @param[in] client           The main AQMS database.
/// @param[in] eventIdentifier  The event to accept.
/// @param[in] logger           Where the outcome is recorded.  Borrowed,
///                             may be null.
/// @result True if AQMS reported the event accepted.
///
/// @note The main database only.  Accepting sets the preferred origin's
///       review flag and bumps the event version; there is no alarm to
///       issue and so no other machine that needs telling.
///
/// @note epref.accept_event answers 1 for an event that was already
///       accepted, so calling this twice is not an error and not a
///       second version bump.
///
/// @throws std::exception if the database cannot be reached or refuses
///         the call.
[[nodiscard]] bool acceptEvent(const Client &client,
                               int64_t eventIdentifier,
                               spdlog::logger *logger = nullptr);

/// @brief Marks an event as unwarranted and asks AQMS to send the
///        cancellation.
/// @param[in] mainClient         The main AQMS database.
/// @param[in] auxiliaryClients   The other AQMS machines, if any.
/// @param[in] eventIdentifier    The event to cancel.
/// @param[in] logger             Where the outcome is recorded.  Borrowed,
///                               may be null.
/// @result True if some database reported the event cancelled.
///
/// @note Harder than accepting, because cancelling has to happen on the
///       machine that raised the alarm - that machine is the one holding
///       the state the cancellation message is sent from.  Which machine
///       that is comes from the origins' subsources, which name the
///       real-time system that made them, and those names are matched
///       against the database aliases.
///
/// @note EVERY origin's subsource, not just the preferred one.  An event
///       that has been lightly processed has Jiggle as its preferred
///       origin's subsource, and Jiggle is not a machine that can cancel
///       anything - the real-time subsource is on one of the other
///       origins, and that is the machine holding the event.
///
/// @note A remote that has no such event is the ORDINARY case, not a
///       fault.  The real-time machines skip event identifier ranges
///       between them, so an event that exists on one genuinely does not
///       exist on the other.  Those attempts are logged quietly and the
///       next candidate is tried.
///
/// @note Falls back to the main database when NO remote succeeded -
///       success, not a name match.  That is what catches the Jiggle case,
///       where no subsource names a remote at all and the event still has
///       to be cancelled somewhere.
///
/// @throws std::exception if the subsources cannot be read, or if the
///         fallback cancel on the main database fails.  A failure on a
///         remote is swallowed by design.
[[nodiscard]] bool cancelEvent(
    const Client &mainClient,
    const std::vector<std::shared_ptr<Client>> &auxiliaryClients,
    int64_t eventIdentifier,
    spdlog::logger *logger = nullptr);
}
#endif

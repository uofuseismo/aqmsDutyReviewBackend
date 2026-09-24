#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_ACTIONS_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_ACTIONS_HPP
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
#include <spdlog/logger.h>
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief The solution the analyst reviewed - what the event's preferred
///        origin, preferred magnitude and type were when they looked at
///        it.
/// @note An unset identifier means the event had none, not that the
///       caller does not care.  Unset matches only an unset column.
struct ExpectedSolution
{
    /// event.prefor.
    std::optional<std::int64_t> preferredOriginIdentifier;
    /// event.prefmag.
    std::optional<std::int64_t> preferredMagnitudeIdentifier;
    /// event.etype.  Compared as the enum, because that is all the
    /// analyst saw: every code this backend does not map reads as Unknown.
    Event::EventType eventType{Event::EventType::Unknown};

    bool operator==(const ExpectedSolution &) const = default;
};

/// @brief What an action did.
enum class ActionOutcome
{
    Done,           /*!< AQMS reported the action done. */
    DoesNotExist,   /*!< The main database has no such event. */
    Refused,        /*!< The event exists and AQMS would not do it. */
    SolutionChanged /*!< The event's preferred origin, preferred magnitude
                         or type is no longer what the analyst reviewed,
                         so nothing was done. */
};

/// @brief Marks an event as one a human has reviewed and stands behind.
/// @param[in] client           The main AQMS database.
/// @param[in] eventIdentifier  The event to accept.
/// @param[in] expected         The solution the analyst reviewed.
/// @param[in] logger           Where the outcome is recorded.  Borrowed,
///                             may be null.
/// @result Done if AQMS reported the event accepted.
///
/// @note The check against \c expected and the accept are atomic: nothing
///       can change the preferred origin, preferred magnitude or type
///       between them.
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
[[nodiscard]] ActionOutcome acceptEvent(const Client &client,
                                        int64_t eventIdentifier,
                                        const ExpectedSolution &expected,
                                        spdlog::logger *logger = nullptr);

/// @brief Marks an event as unwarranted and asks AQMS to send the
///        cancellation.
/// @param[in] mainClient         The main AQMS database.
/// @param[in] auxiliaryClients   The other AQMS machines, if any.
/// @param[in] eventIdentifier    The event to cancel.
/// @param[in] expected           The solution the analyst reviewed.
/// @param[in] logger             Where the outcome is recorded.  Borrowed,
///                               may be null.
/// @result Done if some database reported the event cancelled.
///
/// @note The check against \c expected is made on the main database
///       before any cancel is attempted, but is NOT atomic with it: the
///       cancel usually runs on another machine.  A change in the moment
///       between the two goes undetected.
///
/// @note DoesNotExist, without trying anywhere, if the main database has
///       no such event.
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
[[nodiscard]] ActionOutcome cancelEvent(
    const Client &mainClient,
    const std::vector<std::shared_ptr<Client>> &auxiliaryClients,
    int64_t eventIdentifier,
    const ExpectedSolution &expected,
    spdlog::logger *logger = nullptr);
}
#endif

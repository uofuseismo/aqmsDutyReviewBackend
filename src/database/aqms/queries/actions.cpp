#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/actions.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/eventQueries.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

/*
DROP FUNCTION IF EXISTS epref.accept_event(p_evid Event.evid%TYPE);
CREATE OR REPLACE FUNCTION epref.accept_event(p_evid Event.evid%TYPE)  RETURNS BIGINT AS $$
  DECLARE
    v_status BIGINT := 0;
    v_orid Origin.orid%TYPE := NULL;
    v_flag Origin.rflag%TYPE := NULL;
  BEGIN
    --
    IF (p_evid IS NULL OR p_evid < 1) THEN
      RETURN 0;
    END IF;
    --
    SELECT event.prefor, COALESCE(origin.rflag,'$') INTO v_orid, v_flag FROM event LEFT OUTER JOIN
        origin ON event.prefor = origin.orid WHERE event.evid = p_evid;
    --
    -- Should generate alarm-type update actions
    IF (v_flag <> 'H') THEN -- update only if different -aww 2008/02/28
      UPDATE origin SET rflag = 'H' WHERE orid = v_orid;
      --
      GET DIAGNOSTICS v_status = ROW_COUNT;
      --
      IF (v_status > 0) THEN
        v_status := epref.bump_version(p_evid); -- due to rflag change
        IF ( v_status > 0 ) THEN
            v_status := v_status + 1;
        END IF;
      END IF;
      --
    ELSE
      v_status := 1; -- already set
    END IF;
    --
    RETURN v_status;
    --
  EXCEPTION WHEN NO_DATA_FOUND THEN
    RETURN -1;
  END;
$$ LANGUAGE plpgsql;

DROP FUNCTION IF EXISTS epref.cancel_event(p_evid Event.evid%TYPE);
-- Just posts to state that causes cancellation messages to be sent.
CREATE OR REPLACE FUNCTION epref.cancel_event(p_evid Event.evid%TYPE) RETURNS BIGINT AS $$
  DECLARE
    v_status BIGINT := 0;
  BEGIN
--   
    -- POST for alarm processing
    v_status := PCS.POST_ID('TPP', 'TPP', p_evid, 'DELETED', 100);
    --
    IF (v_status > 0) THEN
      v_status := epref.setRFlag(p_evid, 'C');
    END IF;
    --
    RETURN v_status;
    --
END;
$$ LANGUAGE plpgsql;

create or replace FUNCTION epref.bump_version(p_evid Event.evid%TYPE, p_commit INTEGER) RETURNS INTEGER AS $$
DECLARE
    v_status  INTEGER := 0;
    v_version Event.version%TYPE := NULL;
BEGIN
    SELECT version INTO v_version FROM event WHERE evid = p_evid;
    IF (v_version IS NULL) THEN
          --v_version := 1;
          v_version := 0; -- aww 04/23/2008 NCEDC wants this
    END IF;
    --
    v_version := v_version + 1;
    --
    UPDATE event set version = v_version where evid = p_evid;
    GET DIAGNOSTICS v_status = ROW_COUNT;
    --
    IF (v_status > 0) THEN
        --
        RETURN v_version;
        --
    END IF;
    RETURN -1;
END 
$$ LANGUAGE plpgsql; 
        
-- this version only takes evid as argument and commits upon success.
create or replace FUNCTION epref.bump_version(p_evid Event.evid%TYPE) RETURNS INTEGER AS $$
BEGIN
    RETURN epref.bump_version(p_evid,1);
END 
$$ LANGUAGE plpgsql;


CREATE OR REPLACE FUNCTION pcs.putState
  ( p_group pcs_state.controlgroup%TYPE,
    p_source pcs_state.sourcetable%TYPE,
    p_id pcs_state.id%TYPE,
    p_state pcs_state.state%TYPE,
    p_rank pcs_state.rank%TYPE) RETURNS INTEGER AS $$
 BEGIN
   -- RAISE NOTICE USING MESSAGE = 'putState:p_group: ' || p_group || ' p_source: ' || p_source || ' p_state: ' || p_state || ' p_rank: ' || p_rank;
   RETURN pcs.post_id(p_group, p_source, p_id, p_state, p_rank, 0.0, 1.0); -- result 0, commit
 END
$$ LANGUAGE plpgsql;


DROP FUNCTION IF EXISTS epref.delete_event(p_evid Event.evid%TYPE);
-- Does NOT actually remove the record, just sets selectflag = 0
-- and runs epref.cancel_event (posts to PCS system).
CREATE OR REPLACE FUNCTION epref.delete_event(p_evid Event.evid%TYPE) RETURNS BIGINT AS $$
  DECLARE
    v_status BIGINT := 0;
    v_flag event.selectflag%TYPE := NULL;
  BEGIN
    SELECT COALESCE(selectflag, -1) INTO v_flag FROM event WHERE evid = p_evid;
    IF (v_flag <> 0) THEN
       -- update only when not 0 already
       UPDATE event SET selectflag = 0 WHERE evid = p_evid;
       GET DIAGNOSTICS v_status = ROW_COUNT;
    ELSE
        -- already set to 0
        v_status := 1;
    END IF;

    -- POST to generate alarm delete/cancel messages
    IF (v_status > 0) THEN
      v_status := v_status + epref.cancel_event(p_evid);
    END IF;

    RETURN v_status;

  EXCEPTION WHEN NO_DATA_FOUND THEN
    RETURN -1;
  END;
$$ LANGUAGE plpgsql;

CREATE OR REPLACE FUNCTION accept_trigger(p_evid Event.evid%TYPE) RETURNS INTEGER AS $$
  DECLARE
    v_status INTEGER := 0;
    v_select Event.selectflag%TYPE := NULL;
    v_orid Origin.orid%TYPE := NULL;
    v_flag Origin.rflag%TYPE := NULL;
  BEGIN
    --
    IF (p_evid IS NULL) THEN
      RETURN 0;
    END IF;
    --
    SELECT coalesce(prefor, 0), coalesce(selectflag, -1), coalesce(rflag, '$') INTO v_orid, v_select, v_flag
        FROM event LEFT OUTER JOIN origin ON event.prefor = origin.orid WHERE event.evid = p_evid;
    --
    IF (v_select <> 1) THEN -- update only if different -aww 2008/02/28
      UPDATE event Set selectFlag = 1 WHERE evid = p_evid;
      GET DIAGNOSTICS v_status = ROW_COUNT;
      v_status := v_status + 1;
    ELSE
      v_status := 1; -- already set
    END IF;
    --
    IF (v_orid > 0 AND v_flag <> 'H') THEN -- update only if different -aww 2008/02/28
      UPDATE origin SET origin.rflag = 'H' WHERE orid = v_orid; --  origin.bogusflag = 0
      GET DIAGNOSTICS v_status = ROW_COUNT;
      v_status := v_status + 2;
    ELSE
      v_status := 1; -- already set
    END IF;
    --
    IF (v_status > 2) THEN
      --does commit
      v_status := bump_version(p_evid); -- due to rflag change
    END IF;
    --
    RETURN v_status;
    --
END
$$ language plpgsql;

CREATE OR REPLACE FUNCTION EPREF.setPreforGtype(p_evid Event.evid%TYPE, p_type Origin.gtype%TYPE) RETURNS INTEGER AS $$
  DECLARE
    v_status INTEGER := 0;
    v_type Origin.gtype%TYPE := NULL;
    v_prefor BIGINT := NULL;
  BEGIN
    --
    IF (p_evid IS NULL OR p_type IS NULL) THEN
      RETURN 0;
    END IF;
    --
    SELECT coalesce(gtype, ''),prefor INTO v_type, v_prefor FROM trinetdb.origin, trinetdb.event  WHERE event.evid = p_evid AND origin.orid=event.prefor;
    --
    IF (v_type <> p_type) THEN -- update only if different -aww 2008/02/28
      UPDATE trinetdb.origin SET gtype = p_type WHERE orid = v_prefor;
      GET DIAGNOSTICS v_status = ROW_COUNT;
      --
      IF (v_status > 0) THEN
        --does commit
        v_status := EPREF.bump_version(p_evid); -- due to gtype change
      END IF;
      --
    ELSE
      v_status := 1; -- already set
    END IF;
    --
    RETURN v_status;
    --
  EXCEPTION WHEN NO_DATA_FOUND THEN
    RETURN -1;
  END;
$$ language plpgsql;

-- Set flags indicating what this event is:
-- See constraints on Event.etype for options.
CREATE OR REPLACE FUNCTION EPREF.setEventType(p_evid Event.evid%TYPE, p_type Event.etype%TYPE) RETURNS INTEGER AS $$
  DECLARE
    v_status INTEGER := 0;
    v_type Event.etype%TYPE := NULL;
  BEGIN
    --
    IF (p_evid IS NULL OR p_type IS NULL) THEN
      RETURN 0;
    END IF;
    --
    SELECT coalesce(etype, '') INTO v_type FROM trinetdb.event WHERE evid = p_evid;
    --
    IF (v_type <> p_type) THEN -- update only if different -aww 2008/02/28
      UPDATE trinetdb.event SET etype = p_type WHERE evid = p_evid;
      GET DIAGNOSTICS v_status = ROW_COUNT;
      --
      IF (v_status > 0) THEN
        --does commit
        v_status := EPREF.bump_version(p_evid); -- due to etype change
      END IF;
      --
    ELSE
      v_status := 1; -- already set
    END IF;
    --
    RETURN v_status;
    --
  EXCEPTION WHEN NO_DATA_FOUND THEN
    RETURN -1;
  END;
$$ language plpgsql;



 */

namespace
{

constexpr std::string_view ACCEPT_EVENT_QUERY
{
R"""(
SELECT epref.accept_event($1);
)"""
};

/// FOR UPDATE because the accept runs in the same transaction: prefor,
/// prefmag and etype are columns of this row, so holding it is what keeps
/// the solution from changing between the check and the accept.
constexpr std::string_view LOCK_SOLUTION_QUERY
{
R"""(
SELECT prefor, prefmag, etype FROM event WHERE evid = $1 FOR UPDATE;
)"""
};

/// No lock - the cancel it guards runs on another machine, so there is no
/// transaction to hold it across.
constexpr std::string_view READ_SOLUTION_QUERY
{
R"""(
SELECT prefor, prefmag, etype FROM event WHERE evid = $1;
)"""
};

constexpr std::string_view POST_ACCEPT_QUERY
{
R"""(
SELECT pcs.putState($1, $2, $3, $4, $5);
)"""
};

constexpr std::string_view CANCEL_EVENT_QUERY
{
R"""(
SELECT epref.cancel_event($1);
)"""
};

/// Who "made" the event, per origin.
///
/// EVERY origin, not just the preferred one.  A lightly processed event
/// has Jiggle as its preferred subsource, and Jiggle cannot cancel
/// anything; the real-time machine that actually holds the event is named
/// on one of the other origins.  Reading only event.prefor is why a cancel
/// on such an event silently does nothing.
constexpr std::string_view GET_ORIGIN_SUBSOURCES
{
R"""(
SELECT DISTINCT origin.subsource as subsource
  FROM event
 INNER JOIN origin ON origin.evid = event.evid
 WHERE event.evid = $1
   AND origin.subsource IS NOT NULL;
)"""
};

/// Sets the event type from the subnet trigger.
constexpr std::string_view SET_SUBNET_TRIGGER_EVENT_TYPE_QUERY
{
R"""(
SELECT epref.setEventType($1, $2);
)"""
};

constexpr std::string_view SET_SUBNET_TRIGGER_GEOGRAPHIC_TYPE_QUERY
{
R"""(
SELECT epref.setPreforGtype($1, $2);
)"""
};

constexpr std::string_view SET_SUBNET_TRIGGER_BUMP_VERSION_QUERY
{
R"""(
SELECT epref.bump_version($1);
)"""
};

/// "Deletes" a subnet trigger.  This doesn't actually delete the subnet trigger
/// (see above).
constexpr std::string_view DELETE_SUBNET_TRIGGER_QUERY
{
R"""(
SELECT epref.delete_event($1);
)"""
};

/// FOR UPDATE for the same reason as LOCK_SOLUTION_QUERY: the trigger
/// actions run in the transaction that holds it, so the check that this is
/// still an unreviewed trigger cannot go stale before they write.
constexpr std::string_view LOCK_SUBNET_TRIGGER_QUERY
{
R"""(
SELECT etype, version FROM event WHERE evid = $1 FOR UPDATE;
)"""
};

/// What the trigger looked like under the lock.
struct LockedTrigger
{
    bool isSubnetTrigger{false};
    std::int64_t version{0};
};

/// What happened when an action was run against a locked trigger.
struct TriggerActionResult
{
    std::optional<LockedTrigger> trigger;
    bool done{false};
};

/// @brief Locks a trigger, checks it still is one, and runs an action on
///        it in the same transaction.
/// @param[in] action  Runs only on a trigger.  Returns true to commit;
///                    false rolls back everything it did.
/// @note The rollback is the point.  An action that is several statements
///       - set the type, then the geographic type - either lands whole or
///       not at all, so a failure half way never leaves an event retyped
///       and still marked as the wrong kind of origin.
[[nodiscard]] TriggerActionResult runOnSubnetTrigger(
    const DB::Client &client,
    const int64_t eventIdentifier,
    const std::string_view label,
    const std::function<bool (pqxx::work &, const LockedTrigger &)> &action)
{
    // Assigned rather than accumulated: Client::execute may run the
    // operation twice after re-dialling a dropped connection.
    TriggerActionResult result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            result = TriggerActionResult{};
            const auto rows
                = transaction.exec(std::string {::LOCK_SUBNET_TRIGGER_QUERY},
                                   pqxx::params{eventIdentifier});
            // Returning without a commit rolls back and releases the lock.
            if (rows.empty()){return;}
            const auto row = rows[0];
            LockedTrigger trigger;
            trigger.isSubnetTrigger
                = !row.at("etype").is_null()
               && row.at("etype").as<std::string> ()
                  == eventTypeToCode(Event::EventType::SubnetTrigger);
            trigger.version = row.at("version").is_null()
                            ? 0 : row.at("version").as<std::int64_t> ();
            result.trigger = trigger;
            if (!trigger.isSubnetTrigger){return;}
            if (!action(transaction, trigger)){return;}
            transaction.commit();
            result.done = true;
        },
        label);
    return result;
}

/// @brief Turns a trigger action's result into an outcome, and logs it.
[[nodiscard]] ActionOutcome toTriggerOutcome(
    const TriggerActionResult &result,
    const DB::Client &client,
    const int64_t eventIdentifier,
    const std::string_view verb,
    spdlog::logger *logger)
{
    if (!result.trigger)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger, "{} has no subnet trigger {} to {}",
                               client.getName(), eventIdentifier, verb);
        }
        return ActionOutcome::DoesNotExist;
    }
    if (!result.trigger->isSubnetTrigger)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_INFO(logger,
                               "Not going to {} event {} - it is no longer a "
                               "subnet trigger",
                               verb, eventIdentifier);
        }
        return ActionOutcome::SolutionChanged;
    }
    if (!result.done)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger, "{} would not {} subnet trigger {}",
                               client.getName(), verb, eventIdentifier);
        }
        return ActionOutcome::Refused;
    }
    return ActionOutcome::Done;
}


/// @result The event's preferred origin, preferred magnitude and type,
///         or nullopt if there is no such event.
[[nodiscard]] std::optional<ExpectedSolution> readSolution(
    pqxx::work &transaction,
    const std::string_view query,
    const int64_t eventIdentifier,
    spdlog::logger *logger)
{
    const auto rows = transaction.exec(std::string {query},
                                       pqxx::params{eventIdentifier});
    if (rows.empty()){return std::nullopt;}
    const auto row = rows[0];
    const auto readIdentifier
        = [&row](const char *column) -> std::optional<std::int64_t>
          {
              if (row.at(column).is_null()){return std::nullopt;}
              return row.at(column).as<std::int64_t> ();
          };
    // Unknown for an unmapped code, exactly as the event detail reads it -
    // otherwise an analyst shown "unknown" could never match.
    auto eventType = Event::EventType::Unknown;
    try
    {
        eventType = eventTypeFromCode(row.at("etype").as<std::string> ());
    }
    catch (const std::invalid_argument &e)
    {
        if (logger)
        {
            SPDLOG_LOGGER_DEBUG(logger,
                "Failed to read event type because {}; setting to unknown",
                e.what());
        }
    }
    return ExpectedSolution{readIdentifier("prefor"),
                            readIdentifier("prefmag"),
                            eventType};
}

[[nodiscard]] std::string toString(const std::optional<std::int64_t> &value)
{
    return value ? std::to_string(*value) : std::string {"none"};
}

/// @brief Records why an action was refused as stale.
void logSolutionChanged(spdlog::logger *logger,
                        const std::string_view verb,
                        const int64_t eventIdentifier,
                        const ExpectedSolution &expected,
                        const ExpectedSolution &current)
{
    if (logger == nullptr){return;}
    SPDLOG_LOGGER_INFO(logger,
                       "Not {} event {} - the analyst reviewed preferred "
                       "origin {} and magnitude {}, but it is now origin {} "
                       "and magnitude {}{}",
                       verb, eventIdentifier,
                       ::toString(expected.preferredOriginIdentifier),
                       ::toString(expected.preferredMagnitudeIdentifier),
                       ::toString(current.preferredOriginIdentifier),
                       ::toString(current.preferredMagnitudeIdentifier),
                       expected.eventType == current.eventType
                           ? "" : ", and its type has changed");
}

/// @brief Reads the origin subsources.
[[nodiscard]] std::set<std::string> readSubsources(const pqxx::result &rows,
                                                   spdlog::logger *logger)
{
    std::set<std::string> subsources;
    for (const auto &row : rows)
    {
        try
        {
            if (row.at("subsource").is_null()){continue;}
            auto subsource = row.at("subsource").as<std::string> ();
            if (subsource.empty()){continue;}
            subsources.insert(std::move(subsource));
        }
        catch (const std::exception &e)
        {
            if (logger != nullptr)
            {
                SPDLOG_LOGGER_ERROR(logger,
                                    "Failed to read a subsource because {}",
                                    std::string {e.what()});
            }
        }
    }
    return subsources;
}

/// @brief Case-insensitive comparison.
/// @note The subsources are written in upper case - RTT - and the aliases
///       in lower - rtt - so an exact match would never fire.
[[nodiscard]] bool namesMatch(std::string left, std::string right)
{
    const auto fold
        = [](std::string &text)
          {
              std::transform(text.begin(), text.end(), text.begin(),
                             [](const unsigned char c)
                             {
                                 return static_cast<char> (std::tolower(c));
                             });
          };
    fold(left);
    fold(right);
    return left == right;
}

/// @brief Runs one of the epref functions and reports what it answered.
/// @result True if the function returned a positive status.
/// @note These functions answer with a status rather than throwing:
///       positive for done, zero for "could not", -1 for no such event.
///       A zero is not an exception and must not be read as success.
[[nodiscard]] bool runAction(const DB::Client &client,
                             const std::string_view query,
                             const int64_t eventIdentifier)
{
    // Assigned rather than accumulated: Client::execute may run the
    // operation twice after re-dialling a dropped connection.
    bool succeeded{false};
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            const auto status
                = transaction.query_value<long long>
                  (std::string {query}, pqxx::params{eventIdentifier});
            transaction.commit();
            succeeded = (status > 0);
        },
        query);
    return succeeded;
}

/// The PCS post that follows an accept.
///
/// epref.accept_event does NOT post - it flips origin.rflag to 'H' and
/// bumps the version, and that is all.  epref.cancel_event, by contrast,
/// posts 'DELETED' itself before setting its flag.  So the post lives out
/// here for accept and nowhere for cancel, which is exactly what the PHP
/// does and is easy to misread as an inconsistency.
///
/// TPP/TPP/100 are the PCS alarm-processing defaults, straight from
/// quickPost.
constexpr std::string_view PCS_CONTROL_GROUP{"TPP"};
constexpr std::string_view PCS_SOURCE_TABLE{"TPP"};
constexpr std::string_view PCS_FINALIZE_STATE{"FINALIZE"};
constexpr int PCS_RANK{100};

/// @brief Posts a state to PCS so the alarm processes pick the event up.
/// @result True if PCS accepted the post.
/// @note Its OWN transaction, deliberately separate from the action that
///       preceded it.  Two reasons.  The accept must stand even if this
///       fails - an analyst's decision should not be undone because a
///       notification did not go out, and re-accepting is a no-op so a
///       retry costs nothing.  And post_id is the statement worth being
///       careful with: running it inside the accept's transaction would
///       hold the write lock on the origin row for however long the post
///       takes, which is the wrong row to be sitting on.
[[nodiscard]] bool postState(const DB::Client &client,
                             const int64_t eventIdentifier,
                             const std::string_view state)
{
    // Assigned rather than accumulated: Client::execute may run the
    // operation twice after re-dialling a dropped connection.
    bool succeeded{false};
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            const auto status
                = transaction.query_value<long long>
                  (std::string {::POST_ACCEPT_QUERY},
                   pqxx::params{std::string {::PCS_CONTROL_GROUP},
                                std::string {::PCS_SOURCE_TABLE},
                                eventIdentifier,
                                std::string {state},
                                ::PCS_RANK});
            transaction.commit();
            succeeded = (status > 0);
        },
        ::POST_ACCEPT_QUERY);
    return succeeded;
}

}

ActionOutcome AQMSDutyReviewBackend::Database::AQMS::acceptEvent(
    const DB::Client &client,
    const int64_t eventIdentifier,
    const ExpectedSolution &expected,
    spdlog::logger *logger)
{
    // Assigned rather than accumulated: Client::execute may run the
    // operation twice after re-dialling a dropped connection.
    std::optional<ExpectedSolution> current;
    bool accepted{false};
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            accepted = false;
            current = ::readSolution(transaction, ::LOCK_SOLUTION_QUERY,
                                      eventIdentifier, logger);
            // Returning without a commit rolls back and releases the lock.
            if (!current || *current != expected){return;}
            const auto status
                = transaction.query_value<long long>
                  (std::string {::ACCEPT_EVENT_QUERY},
                   pqxx::params{eventIdentifier});
            transaction.commit();
            accepted = (status > 0);
        },
        ::ACCEPT_EVENT_QUERY);
    if (current && *current != expected)
    {
        ::logSolutionChanged(logger, "accepting", eventIdentifier,
                             expected, *current);
        return ActionOutcome::SolutionChanged;
    }
    if (!current)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger, "{} has no event {} to accept",
                               client.getName(), eventIdentifier);
        }
        return ActionOutcome::DoesNotExist;
    }
    if (!accepted)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger,
                               "{} would not accept event {}",
                               client.getName(), eventIdentifier);
        }
        // Nothing to post about: the event was not accepted, so telling
        // the alarm processes to refresh its products would be announcing
        // a decision nobody made.
        return ActionOutcome::Refused;
    }
    if (logger != nullptr)
    {
        SPDLOG_LOGGER_INFO(logger, "Accepted event {} on {}",
                           eventIdentifier, client.getName());
    }

    // The post is what makes the accept visible outside the database -
    // it refreshes the products and resends the notifications.  Its
    // failure is reported but does not fail the accept, matching the PHP:
    // the event IS accepted at this point and saying otherwise would have
    // an analyst accept it again to fix something already done.
    try
    {
        if (::postState(client, eventIdentifier, ::PCS_FINALIZE_STATE))
        {
            if (logger != nullptr)
            {
                SPDLOG_LOGGER_INFO(logger,
                                   "Posted {} for event {} on {}",
                                   ::PCS_FINALIZE_STATE, eventIdentifier,
                                   client.getName());
            }
        }
        else if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger,
                               "Event {} was accepted on {} but PCS refused "
                               "the {} post - products and notifications may "
                               "not have refreshed",
                               eventIdentifier, client.getName(),
                               ::PCS_FINALIZE_STATE);
        }
    }
    catch (const std::exception &e)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_ERROR(logger,
                                "Event {} was accepted on {} but the {} post "
                                "threw because {} - products and "
                                "notifications may not have refreshed",
                                eventIdentifier, client.getName(),
                                ::PCS_FINALIZE_STATE, std::string {e.what()});
        }
    }
    return ActionOutcome::Done;
}

ActionOutcome AQMSDutyReviewBackend::Database::AQMS::cancelEvent(
    const DB::Client &mainClient,
    const std::vector<std::shared_ptr<DB::Client>> &auxiliaryClients,
    const int64_t eventIdentifier,
    const ExpectedSolution &expected,
    spdlog::logger *logger)
{
    // Who made the origins, and what the analyst's solution has become.
    // Read from the main database because that is the archive - it has
    // every origin, including ones made on machines this backend may not
    // even be configured to reach - and it is where the analyst's view of
    // the event came from.
    std::optional<ExpectedSolution> current;
    std::set<std::string> subsources;
    mainClient.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            current = ::readSolution(transaction, ::READ_SOLUTION_QUERY,
                                      eventIdentifier, logger);
            auto rows = transaction.exec(std::string {::GET_ORIGIN_SUBSOURCES},
                                         pqxx::params{eventIdentifier});
            transaction.commit();
            subsources = ::readSubsources(rows, logger);
        },
        ::GET_ORIGIN_SUBSOURCES);
    if (!current)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger, "{} has no event {} to cancel",
                               mainClient.getName(), eventIdentifier);
        }
        return ActionOutcome::DoesNotExist;
    }
    if (*current != expected)
    {
        ::logSolutionChanged(logger, "cancelling", eventIdentifier,
                             expected, *current);
        return ActionOutcome::SolutionChanged;
    }

    // Try every remote a subsource names.  A remote that does not have the
    // event is ORDINARY - the real-time machines skip identifier ranges
    // between them, so an event on one genuinely is not on the other - so
    // a failure here is not reported as one and the next candidate is
    // tried.
    for (const auto &subsource : subsources)
    {
        for (const auto &client : auxiliaryClients)
        {
            if (client == nullptr){continue;}
            if (!::namesMatch(subsource, client->getName())){continue;}
            try
            {
                if (::runAction(*client, ::CANCEL_EVENT_QUERY,
                                eventIdentifier))
                {
                    if (logger != nullptr)
                    {
                        SPDLOG_LOGGER_INFO(logger,
                                           "Cancelled event {} on {}, which "
                                           "subsource {} named",
                                           eventIdentifier,
                                           client->getName(), subsource);
                    }
                    return ActionOutcome::Done;
                }
                if (logger != nullptr)
                {
                    SPDLOG_LOGGER_DEBUG(logger,
                                        "{} does not hold event {} - "
                                        "expected, the machines skip "
                                        "identifier ranges",
                                        client->getName(), eventIdentifier);
                }
            }
            catch (const std::exception &e)
            {
                if (logger != nullptr)
                {
                    SPDLOG_LOGGER_WARN(logger,
                                       "Could not cancel event {} on {} "
                                       "because {} - trying elsewhere",
                                       eventIdentifier, client->getName(),
                                       std::string {e.what()});
                }
            }
        }
    }

    // Nothing remote took it.  That is the ordinary path for an event
    // whose subsources are all post-processing - Jiggle names no machine
    // that can cancel - and for a deployment with no remotes configured.
    if (logger != nullptr)
    {
        if (subsources.empty())
        {
            SPDLOG_LOGGER_WARN(logger,
                               "No subsources on event {} - cancelling on {}",
                               eventIdentifier, mainClient.getName());
        }
        else if (auxiliaryClients.empty())
        {
            SPDLOG_LOGGER_INFO(logger,
                               "No remote databases configured - cancelling "
                               "event {} on {}",
                               eventIdentifier, mainClient.getName());
        }
        else
        {
            SPDLOG_LOGGER_INFO(logger,
                               "No remote cancelled event {} - falling back "
                               "to {}",
                               eventIdentifier, mainClient.getName());
        }
    }
    const auto cancelled = ::runAction(mainClient, ::CANCEL_EVENT_QUERY,
                                       eventIdentifier);
    if (!cancelled && logger != nullptr)
    {
        SPDLOG_LOGGER_ERROR(logger,
                            "{} would not cancel event {} either",
                            mainClient.getName(), eventIdentifier);
    }
    return cancelled ? ActionOutcome::Done : ActionOutcome::Refused;
}

ActionOutcome AQMSDutyReviewBackend::Database::AQMS::deleteSubnetTrigger(
    const DB::Client &mainClient,
    const int64_t eventIdentifier,
    spdlog::logger *logger)
{
    // The type check and the delete share a transaction and the row lock.
    // epref.delete_event checks neither the type nor that the event exists
    // - its SELECT INTO is not STRICT, so a missing evid falls through to
    // "already deleted" and posts a cancel anyway - so this check is the
    // only thing standing between a stale trigger list and deleting an
    // event an analyst has since typed as real.
    std::optional<ExpectedSolution> current;
    bool deleted{false};
    mainClient.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            deleted = false;
            current = ::readSolution(transaction, ::LOCK_SOLUTION_QUERY,
                                     eventIdentifier, logger);
            // Returning without a commit rolls back and releases the lock.
            if (!current
             || current->eventType != Event::EventType::SubnetTrigger)
            {
                return;
            }
            const auto status
                = transaction.query_value<long long>
                  (std::string {::DELETE_SUBNET_TRIGGER_QUERY},
                   pqxx::params{eventIdentifier});
            transaction.commit();
            deleted = (status > 0);
        },
        ::DELETE_SUBNET_TRIGGER_QUERY);
    if (!current)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger, "{} has no subnet trigger {} to delete",
                               mainClient.getName(), eventIdentifier);
        }
        return ActionOutcome::DoesNotExist;
    }
    if (current->eventType != Event::EventType::SubnetTrigger)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_INFO(logger,
                               "Not deleting event {} - it is no longer a "
                               "subnet trigger",
                               eventIdentifier);
        }
        return ActionOutcome::SolutionChanged;
    }
    if (!deleted)
    {
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_WARN(logger, "{} would not delete subnet trigger {}",
                               mainClient.getName(), eventIdentifier);
        }
        return ActionOutcome::Refused;
    }
    if (logger != nullptr)
    {
        SPDLOG_LOGGER_INFO(logger, "Deleted subnet trigger {} on {}",
                           eventIdentifier, mainClient.getName());
    }
    return ActionOutcome::Done;
}

ActionOutcome AQMSDutyReviewBackend::Database::AQMS::setSubnetTriggerEventType(
    const DB::Client &mainClient,
    const int64_t eventIdentifier,
    const AQMS::Event::EventType eventType,
    const AQMS::Origin::GeographicType geographicType,
    spdlog::logger *logger)
{
    if (eventType == Event::EventType::SubnetTrigger)
    {
        throw std::invalid_argument(
            "A subnet trigger cannot be retyped as a subnet trigger");
    }
    // Both throw on a type with no code, before anything is locked.
    const auto eventTypeCode = eventTypeToCode(eventType);
    const auto geographicTypeCode = geographicTypeToCode(geographicType);

    const auto result = ::runOnSubnetTrigger(
        mainClient, eventIdentifier, ::SET_SUBNET_TRIGGER_EVENT_TYPE_QUERY,
        [&](pqxx::work &transaction, const LockedTrigger &)
        {
            const auto typeStatus
                = transaction.query_value<long long>
                  (std::string {::SET_SUBNET_TRIGGER_EVENT_TYPE_QUERY},
                   pqxx::params{eventIdentifier, eventTypeCode});
            if (typeStatus <= 0){return false;}
            const auto geographicStatus
                = transaction.query_value<long long>
                  (std::string {::SET_SUBNET_TRIGGER_GEOGRAPHIC_TYPE_QUERY},
                   pqxx::params{eventIdentifier, geographicTypeCode});
            return geographicStatus > 0;
        });
    const auto outcome
        = ::toTriggerOutcome(result, mainClient, eventIdentifier, "retype",
                             logger);
    if (outcome == ActionOutcome::Done && logger != nullptr)
    {
        SPDLOG_LOGGER_INFO(logger,
                           "Retyped subnet trigger {} as ({}, {}) on {}",
                           eventIdentifier, eventTypeCode, geographicTypeCode,
                           mainClient.getName());
    }
    return outcome;
}

ActionOutcome AQMSDutyReviewBackend::Database::AQMS::acceptSubnetTrigger(
    const DB::Client &mainClient,
    const int64_t eventIdentifier,
    spdlog::logger *logger)
{
    const auto result = ::runOnSubnetTrigger(
        mainClient, eventIdentifier, ::SET_SUBNET_TRIGGER_BUMP_VERSION_QUERY,
        [&](pqxx::work &transaction, const LockedTrigger &trigger)
        {
            // Already past version 0 means already accepted - the trigger
            // list only shows version 0 - so a second accept is a no-op
            // rather than another bump.
            if (trigger.version > 0){return true;}
            const auto version
                = transaction.query_value<long long>
                  (std::string {::SET_SUBNET_TRIGGER_BUMP_VERSION_QUERY},
                   pqxx::params{eventIdentifier});
            return version > 0;
        });
    const auto outcome
        = ::toTriggerOutcome(result, mainClient, eventIdentifier, "accept",
                             logger);
    if (outcome == ActionOutcome::Done && logger != nullptr)
    {
        SPDLOG_LOGGER_INFO(logger, "Accepted subnet trigger {} on {}",
                           eventIdentifier, mainClient.getName());
    }
    return outcome;
}

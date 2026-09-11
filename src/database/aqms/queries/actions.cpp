#include <algorithm>
#include <cctype>
#include <cstdint>
#include <exception>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/actions.hpp"
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


 */

namespace
{

constexpr std::string_view ACCEPT_EVENT_QUERY
{
R"""(
SELECT epref.accept_event($1);
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

bool AQMSDutyReviewBackend::Database::AQMS::acceptEvent(
    const DB::Client &client,
    const int64_t eventIdentifier,
    spdlog::logger *logger)
{
    const auto accepted = ::runAction(client, ::ACCEPT_EVENT_QUERY,
                                      eventIdentifier);
    if (!accepted)
    {
        if (logger != nullptr)
        {
            // epref.accept_event answers 0 for a bad identifier and -1
            // when the event is not there.
            SPDLOG_LOGGER_WARN(logger,
                               "{} would not accept event {} - it may not "
                               "exist there",
                               client.getName(), eventIdentifier);
        }
        // Nothing to post about: the event was not accepted, so telling
        // the alarm processes to refresh its products would be announcing
        // a decision nobody made.
        return false;
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
    return accepted;
}

bool AQMSDutyReviewBackend::Database::AQMS::cancelEvent(
    const DB::Client &mainClient,
    const std::vector<std::shared_ptr<DB::Client>> &auxiliaryClients,
    const int64_t eventIdentifier,
    spdlog::logger *logger)
{
    // Who made the origins.  Read from the main database because that is
    // the archive - it has every origin, including ones made on machines
    // this backend may not even be configured to reach.
    std::set<std::string> subsources;
    mainClient.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            auto rows = transaction.exec(std::string {::GET_ORIGIN_SUBSOURCES},
                                         pqxx::params{eventIdentifier});
            transaction.commit();
            subsources = ::readSubsources(rows, logger);
        },
        ::GET_ORIGIN_SUBSOURCES);

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
                    return true;
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
    return cancelled;
}

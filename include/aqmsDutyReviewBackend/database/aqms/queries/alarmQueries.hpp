#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_ALARM_QUERIES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUERIES_ALARM_QUERIES_HPP
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief One row of ALARM_ACTION: an alarm, and what it was doing.
///
/// A plain struct rather than a class with validation, because there is
/// nothing here to validate.  AQMS runs alarms as a sort of actor state
/// machine and these rows are its bookkeeping; the states and action names
/// are its vocabulary, not ours.  This carries them across to the frontend
/// unexamined - inventing an enum of the states we happen to have seen
/// would only mean a new one from AQMS became a parse failure here.
struct AlarmAction
{
    /// Which database this row came from, as Client::getName() reports it.
    /// @note Not a column - ALARM_ACTION records what ran and when, never
    ///       where.  An event picks up alarms on more than one database
    ///       over its life: an ancillary system seeds it and runs a few,
    ///       the event replicates to post-processing, is refined, and runs
    ///       more there later.  So the rows have to be gathered from every
    ///       database and this is the only thing that says which is which.
    std::string database;
    /// The event this ran against.
    std::int64_t eventIdentifier{0};
    /// What ran - ALARM_ACTION.
    std::string action;
    /// What it was doing - ACTION_STATE, e.g. "processing", "completed".
    std::string state;
    /// How many times it has run - MODCOUNT.
    int modificationCount{0};
    /// When the row was last touched - MOD_TIME.  Nullable in the schema,
    /// and it is not part of the primary key, so it really can be absent.
    std::optional<std::chrono::seconds> modificationTime;
};

/// @brief Every alarm action recorded for an event.
/// @param[in] client           A client connected to an AQMS database.
/// @param[in] eventIdentifier  The event.
/// @result The rows, ordered by action, then state, then modification
///         count, each tagged with this client's name.  Empty if this
///         database has nothing for the event.
/// @note This answers for ONE database.  An event's full alarm history may
///       be spread over several, so a caller that wants all of it asks
///       each in turn and concatenates - there is no shortcut, because
///       nothing but the rows themselves says where they are.
/// @note Several rows per event is normal, not a duplicate: the primary
///       key is (event_id, alarm_action, action_state, modcount), so one
///       alarm that went from processing to completed leaves a row for
///       each, and a re-run leaves more.  The history is the point.
/// @throws std::runtime_error if the query fails.
[[nodiscard]] std::vector<AlarmAction> queryAlarmActions(
    const Client &client, std::int64_t eventIdentifier);

/// @brief Does this database hold any alarm actions for the event?
/// @result True if it does.
/// @note The cheap probe for "is this database worth asking".  Asking
///       whether a row exists beats dragging rows back from a database
///       that has none.  Note this does not narrow the search to one
///       machine: several may answer true for the same event, and all of
///       them are right.
/// @throws std::runtime_error if the query fails.
[[nodiscard]] bool hasAlarmActions(const Client &client,
                                   std::int64_t eventIdentifier);

/// @brief Every alarm action for an event, gathered from every database.
/// @param[in] clients          The databases to ask, in the order to ask
///                             them.
/// @param[in] eventIdentifier  The event.
/// @param[in] logger           Where a skipped database is recorded.
/// @result The rows from every database that answered, concatenated in
///         client order and each tagged with the database it came from.
/// @note A database that cannot be reached is skipped, not fatal.  The
///       alternative - failing the whole gather - would throw away the
///       history that did come back, and an ancillary machine being down
///       is far more likely than that machine holding the one alarm
///       somebody needed at that moment.  The frontend simply shows no
///       alarms from that computer.
/// @warning The skip is only recorded in the log, so a missing machine is
///          indistinguishable from a machine with nothing to say when
///          looking at the result alone.  That is the deliberate trade;
///          if the frontend ever needs to draw the difference, this has to
///          report which databases were skipped.
/// @note The databases are asked one after another, so an unreachable one
///       costs its connection timeout before the next is tried.  Keep that
///       timeout short on ancillary credentials - several down machines
///       otherwise add up to a visible stall.
[[nodiscard]] std::vector<AlarmAction> queryAlarmActions(
    std::span<const std::shared_ptr<Client>> clients,
    std::int64_t eventIdentifier,
    const std::shared_ptr<spdlog::logger> &logger);
}
#endif

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/alarmQueries.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{

/// @note mod_time is TIMESTAMP WITHOUT TIME ZONE, so EXTRACT(EPOCH FROM ...)
///       reads it as UTC - the convention AQMS writes these with.  Doing
///       the conversion in SQL beats parsing timestamp text in C++.
constexpr std::string_view ALARM_ACTION_QUERY{
    "SELECT event_id, alarm_action, action_state, modcount, "
    "       EXTRACT(EPOCH FROM mod_time)::BIGINT "
    "  FROM alarm_action "
    " WHERE event_id = $1 "
    " ORDER BY alarm_action, action_state, modcount"};

/// @note EXISTS stops at the first matching row rather than counting them
///       all, which is what makes this cheap enough to fire at every
///       candidate database in turn.
constexpr std::string_view HAS_ALARM_ACTION_QUERY{
    "SELECT EXISTS (SELECT 1 FROM alarm_action WHERE event_id = $1)"};

/// @brief Turns one row into an AlarmAction.
[[nodiscard]] AlarmAction readAlarmAction(const pqxx::row_ref &row,
                                          const std::string &database)
{
    AlarmAction alarmAction;
    alarmAction.database = database;
    alarmAction.eventIdentifier = row.at(0).as<std::int64_t> ();
    // VARCHAR, not CHAR, so these are not blank-padded and come across as
    // AQMS wrote them.
    alarmAction.action = row.at(1).as<std::string> ();
    alarmAction.state = row.at(2).as<std::string> ();
    alarmAction.modificationCount = row.at(3).as<int> ();
    if (!row.at(4).is_null())
    {
        alarmAction.modificationTime
            = std::chrono::seconds{row.at(4).as<long long> ()};
    }
    return alarmAction;
}

}

std::vector<AlarmAction>
AQMSDutyReviewBackend::Database::AQMS::queryAlarmActions(
    const DB::Client &client, const std::int64_t eventIdentifier)
{
    // Asked once, outside the lambda: the lambda can run twice on a
    // reconnect and this does not change between attempts.
    const auto database = client.getName();
    std::vector<AlarmAction> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            // Gathered into a local and assigned once: the client re-runs
            // this after re-dialling a dropped connection, and appending
            // straight into the result would double every row.
            std::vector<AlarmAction> rows;
            const pqxx::params parameters{eventIdentifier};
            for (const auto &row
                 : transaction.exec(::ALARM_ACTION_QUERY, parameters))
            {
                rows.push_back(::readAlarmAction(row, database));
            }
            transaction.commit();
            result = std::move(rows);
        },
        ::ALARM_ACTION_QUERY);
    return result;
}

bool AQMSDutyReviewBackend::Database::AQMS::hasAlarmActions(
    const DB::Client &client, const std::int64_t eventIdentifier)
{
    return client.executeScalar<bool> (::HAS_ALARM_ACTION_QUERY,
                                       pqxx::params{eventIdentifier});
}

std::vector<AlarmAction>
AQMSDutyReviewBackend::Database::AQMS::queryAlarmActions(
    const std::span<const std::shared_ptr<DB::Client>> clients,
    const std::int64_t eventIdentifier,
    spdlog::logger *logger)
{
    std::vector<AlarmAction> result;
    for (const auto &client : clients)
    {
        if (client == nullptr){continue;}
        try
        {
            auto actions = queryAlarmActions(*client, eventIdentifier);
            result.insert(result.end(),
                          std::make_move_iterator(actions.begin()),
                          std::make_move_iterator(actions.end()));
        }
        catch (const std::exception &e)
        {
            // Skipped, not fatal.  Failing here would discard the history
            // that did come back from every other database, and a machine
            // being down is far likelier than that machine holding the one
            // alarm somebody needed.  Logged at warning because it is worth
            // noticing when someone asks why a computer stopped appearing.
            SPDLOG_LOGGER_WARN(
                logger,
                "Skipping {} while gathering alarms for event {} because {}",
                client->getName(), eventIdentifier, std::string {e.what()});
        }
    }
    return result;
}

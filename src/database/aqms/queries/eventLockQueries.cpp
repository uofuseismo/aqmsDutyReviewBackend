#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/eventLockQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{
/// @note No WHERE clause on purpose - the table holds one row per locked
///       event and only a handful of analysts are ever on duty, so this is
///       a few rows and narrowing it would cost more than it saved.
constexpr std::string_view EVENT_LOCK_QUERY{
    "SELECT evid, username FROM jasieventlock ORDER BY evid"};

/// Stands in for a lock whose username column is NULL.
constexpr std::string_view UNKNOWN_USER{"unknown"};
}

std::vector<EventLock>
AQMSDutyReviewBackend::Database::AQMS::queryEventLocks(const DB::Client &client)
{
    std::vector<EventLock> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            // Gathered into a local and assigned once: the client re-runs
            // this after re-dialling a dropped connection, and appending
            // straight into the result would list every lock twice.
            std::vector<EventLock> rows;
            for (const auto &row : transaction.exec(::EVENT_LOCK_QUERY))
            {
                // A nameless lock is still a lock.  Skipping the row would
                // report the event as free, which is the one answer that
                // could have two analysts working it at once.  Imputed
                // rather than left blank so the frontend renders it like
                // any other name instead of needing a case for "".
                EventLock lock;
                lock.setEventIdentifier(row.at(0).as<std::int64_t> ());
                lock.setUser(row.at(1).is_null()
                             ? std::string {::UNKNOWN_USER}
                             : row.at(1).as<std::string> ());
                rows.push_back(std::move(lock));
            }
            transaction.commit();
            result = std::move(rows);
        },
        ::EVENT_LOCK_QUERY);
    return result;
}

#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/subnetTriggerQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/subnetTrigger.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{

/// The subnet triggers.  Its own query rather than the catalog's, because
/// almost nothing the catalog selects means anything for a trigger: there
/// is no preferred magnitude, nobody is credited with it, and the position
/// columns hold placeholders.  Selecting them only to discard them would
/// be three joins bought for nothing.
///
/// Ordered like the catalog, evid tiebreaker included - see ORDER_BY_TIME
/// for why datetime alone is not enough.  Nothing hashes this today, but
/// triggers tie on origin time far more readily than located events do,
/// and two callers of the same table disagreeing about order is its own
/// small surprise.
constexpr std::string_view SUBNET_TRIGGER_QUERY
{
R"""(
SELECT event.evid as event_identifier,
       TrueTime.getEpoch(origin.datetime, 'NOMINAL') as origin_time,
       origin.subsource as origin_source
FROM event
INNER JOIN origin
  ON event.prefor = origin.orid
WHERE event.selectflag = 1
  AND origin.datetime BETWEEN TrueTime.nominal2truef($1) AND TrueTime.nominal2Truef($2)
  AND event.etype = 'st'
 ORDER BY origin.datetime DESC, event.evid DESC;
)"""
};

}

std::vector<SubnetTrigger>
AQMSDutyReviewBackend::Database::AQMS::querySubnetTriggers(
    const DB::Client &client,
    const std::chrono::seconds &duration,
    spdlog::logger *logger)
{
    if (duration.count() <= 0)
    {
        throw std::invalid_argument("Duration must be positive");
    }
    const auto endTime
        = std::chrono::duration_cast<std::chrono::seconds>
          (std::chrono::system_clock::now().time_since_epoch());
    const auto startTime = endTime - duration;

    std::vector<SubnetTrigger> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            // Gathered into a local and assigned once: the client re-runs
            // this after re-dialling a dropped connection, and appending
            // straight into the result would double every row.
            std::vector<SubnetTrigger> rows;
            const pqxx::params parameters
            {
                static_cast<double> (startTime.count()),
                static_cast<double> (endTime.count())
            };
            for (const auto &row
                 : transaction.exec(::SUBNET_TRIGGER_QUERY, parameters))
            {
                try
                {
                    SubnetTrigger trigger;
                    trigger.setEventIdentifier(row.at(0).as<std::int64_t> ());
                    const auto timeNanoSeconds
                        = static_cast<int64_t>
                          (std::round(row.at(1).as<double> ()*1.e9));
                    trigger.setTime(
                        std::chrono::nanoseconds {timeNanoSeconds});
                    if (!row.at(2).is_null())
                    {
                        trigger.setOriginSource(
                            row.at(2).as<std::string> ());
                    }
                    rows.push_back(std::move(trigger));
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(logger,
                                       "Skipping a subnet trigger row "
                                       "because {}", std::string {e.what()});
                }
            }
            transaction.commit();
            result = std::move(rows);
        },
        ::SUBNET_TRIGGER_QUERY);
    return result;
}


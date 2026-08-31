#include <exception>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/eventLockQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/eventQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/stationQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

class Database::DatabaseImpl
{
public:
    DatabaseImpl(std::shared_ptr<DB::Client> mainClient,
                 std::vector<std::shared_ptr<DB::Client>> auxiliaryClients,
                 std::shared_ptr<spdlog::logger> logger) :
        mMainClient(std::move(mainClient)),
        mAuxiliaryClients(std::move(auxiliaryClients)),
        mLogger(std::move(logger))
    {
        if (mMainClient == nullptr)
        {
            throw std::invalid_argument("Main database client is null");
        }
        // A null in the auxiliary list is almost certainly a deployment
        // that configured a machine it does not have; dropping it here
        // means nothing downstream has to keep checking.
        std::erase(mAuxiliaryClients, nullptr);
        if (mLogger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
            constexpr const char *loggerName{"AQMSDatabaseConsole"};
            mLogger = spdlog::get(loggerName);
            if (mLogger == nullptr)
            {
                mLogger = spdlog::stdout_color_mt(loggerName);
            }
            // NOLINTEND(misc-include-cleaner)
        }
    }

    std::shared_ptr<DB::Client> mMainClient;
    std::vector<std::shared_ptr<DB::Client>> mAuxiliaryClients;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
};

/// Constructor
Database::Database(std::shared_ptr<DB::Client> mainClient,
                   std::vector<std::shared_ptr<DB::Client>> auxiliaryClients,
                   std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<DatabaseImpl> (std::move(mainClient),
                                          std::move(auxiliaryClients),
                                          std::move(logger)))
{
}

/// Constructor - one database
Database::Database(std::shared_ptr<DB::Client> mainClient,
                   std::shared_ptr<spdlog::logger> logger) :
    Database(std::move(mainClient),
             std::vector<std::shared_ptr<DB::Client>> {},
             std::move(logger))
{
}

/// Stations
auto Database::fetchStations() const
    -> std::expected<std::vector<Station>, QueryError>
{
    try
    {
        return queryStations(*pImpl->mMainClient);
    }
    catch (const std::exception &e)
    {
        // The query itself takes no arguments, so anything that goes wrong
        // here is the database being unreachable or unwell rather than a
        // caller asking for something impossible.
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Could not fetch stations from {} because {}",
                            pImpl->mMainClient->getName(),
                            std::string {e.what()});
        return std::unexpected(QueryError::ConnectionFailed);
    }
}

/// Catalog
auto Database::getCatalog(const std::chrono::seconds &duration) const
    -> std::expected<std::vector<EventSummary>, QueryError>
{
    if (duration.count() <= 0)
    {
        // The caller asked for a window with no time in it, which is a bad
        // request and not a database problem.
        return std::unexpected(QueryError::InvalidArgument);
    }
    try
    {
        return queryEventSummaries(*pImpl->mMainClient, duration,
                                   pImpl->mLogger);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Could not fetch the catalog from {} because {}",
                            pImpl->mMainClient->getName(),
                            std::string {e.what()});
        return std::unexpected(QueryError::ConnectionFailed);
    }
}

/// Event locks
auto Database::getLockedEvents() const
    -> std::expected<std::vector<EventLock>, QueryError>
{
    try
    {
        return queryEventLocks(*pImpl->mMainClient);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Could not fetch event locks from {} because {}",
                            pImpl->mMainClient->getName(),
                            std::string {e.what()});
        return std::unexpected(QueryError::ConnectionFailed);
    }
}

/// Destructor
Database::~Database() = default;

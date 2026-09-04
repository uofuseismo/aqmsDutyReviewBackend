#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
#include "aqmsDutyReviewBackend/database/aqms/waveform.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
#include "aqmsDutyReviewBackend/database/aqms/waveform.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/eventLockQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/eventQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/waveformQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/stationQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/waveformQueries.hpp"
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
        // The archive roots, read once here rather than on every waveform
        // fetch.  Deliberately not fatal: a database that is not up yet is
        // a reason to start anyway and pick them up on first use, not a
        // reason to refuse to boot.  getFileRoot refreshes an empty list
        // the first time it fails to find a root in it.
        try
        {
            mFileRoots = getFileRoots(*mMainClient);
            SPDLOG_LOGGER_INFO(mLogger, "Loaded {} archive file root(s)",
                               mFileRoots.fileRoots.size());
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Could not preload the archive file roots "
                               "because {} - they will be read on demand",
                               std::string {e.what()});
        }
    }

    std::shared_ptr<DB::Client> mMainClient;
    std::vector<std::shared_ptr<DB::Client>> mAuxiliaryClients;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    /// @note Written through a const fetchWaveforms - a unique_ptr yields a
    ///       non-const pImpl through a const method.  That is the point
    ///       here, not an oversight: the roots are a cache, so refreshing
    ///       them does not change what the database says.
    FileRoots mFileRoots;
    /// @brief Guards mFileRoots, which getFileRoot rewrites on a miss.
    ///        It lives here rather than inside FileRoots so that FileRoots
    ///        stays a copyable aggregate; this class is held behind a
    ///        pImpl and was never going to be copied anyway.
    mutable std::mutex mFileRootsMutex;
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
        // The logger is borrowed for the call, not retained, so .get()
        // rather than the shared_ptr itself.
        return queryEventSummaries(*pImpl->mMainClient, duration,
                                   pImpl->mLogger.get());
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
auto Database::getLockedEvents(
    const std::chrono::seconds &catalogDuration) const
    -> std::expected<std::vector<EventLock>, QueryError>
{
    if (catalogDuration.count() <= 0)
    {
        // A window with no time in it is a bad request, not a database
        // problem - the same answer getCatalog gives.
        return std::unexpected(QueryError::InvalidArgument);
    }
    try
    {
        return queryEventLocks(*pImpl->mMainClient, catalogDuration);
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

/// Waveforms
auto Database::fetchWaveforms(
    const int64_t eventIdentifier,
    const std::vector<StreamIdentifier> &identifiers) const
    -> std::expected<std::vector<Waveform>, QueryError>
{
    if (identifiers.empty())
    {
        // Nothing was asked for, which is the caller's mistake and not a
        // database problem.
        return std::unexpected(QueryError::InvalidArgument);
    }
    try
    {
        // The per-stream skipping lives in queryWaveforms: a station that
        // was not running is an ordinary answer, and losing the channels
        // that did come back would be the wrong trade.
        //
        // The lock spans the whole call because the roots are read deep
        // inside it, once per stream.  That serializes waveform fetches,
        // which costs nothing today: Client::execute takes its own mutex,
        // so every query through this one connection was already serial.
        // Splitting this finer only pays off alongside a connection pool.
        const std::scoped_lock lock{pImpl->mFileRootsMutex};
        return queryWaveforms(*pImpl->mMainClient, eventIdentifier,
                              identifiers, pImpl->mFileRoots,
                              pImpl->mLogger.get());
    }
    catch (const pqxx::sql_error &e)
    {
        // NOT skipped.  A function the database refuses to run is a fault,
        // and reporting it as "no waveform data" would have an analyst
        // seeing an empty plot while the real reason sits in the log.
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Waveform query refused by {} because {}",
                            pImpl->mMainClient->getName(),
                            std::string {e.what()});
        return std::unexpected(QueryError::QueryFailed);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Could not fetch waveforms from {} because {}",
                            pImpl->mMainClient->getName(),
                            std::string {e.what()});
        return std::unexpected(QueryError::ConnectionFailed);
    }
}

/// Destructor
Database::~Database() = default;

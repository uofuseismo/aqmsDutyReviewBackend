#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DATABASE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DATABASE_HPP
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Client;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 struct AlarmAction;
 class Event;
 class EventLock;
 class EventSummary;
 class Station;
 class StreamIdentifier;
 class Waveform;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class Database database.hpp
/// @brief Class for interacting with the AQMS database(s).
/// @note When dealing with alarms and actions it is entirely possible the event
///       does not exist in the database.  This is because events are seeded
///       by different computers and different computers only can take alarm
///       actions if they have the event.  Moreover, the alarm tables are not
///       replicated.  Therefore, it is wise to iterate over all possible
///       databases and only declare defeat if all databases fail.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Database
{
public:
    enum class ActionError
    {
        ConnectionFailed,   /*!< The database connection could not be formed. */
        InvalidPermissions, /*!< The database user is unable to perform the
                                 request - likley insufficient permissions. */
        DoesNotExist        /*!< The event does not exist in this database. */
    };
    enum class QueryError
    {
        ConnectionFailed,   /*!< The database connection could not be formed. */
        InvalidArgument     /*!< The query parameters were invalid. */
    };
public:
    /// @brief Constructor.
    /// @param[in] mainClient  The database this application talks to
    ///                        constantly - normally post-processing.
    ///                        Everything except alarms is asked of this one.
    /// @param[in] auxiliaryClients  The other AQMS machines, if any.  Only
    ///                        alarms go here, and only because alarm tables
    ///                        are not replicated.
    /// @param[in] logger      The application logger.
    /// @throws std::invalid_argument if the main client is null.
    /// @note Clients, not credentials: connecting, re-dialling, and how
    ///       long a connection is held are the Client's business, and the
    ///       auxiliary ones want a different answer to the last of those
    ///       than the main one does.  Handing this credentials would mean
    ///       deciding all of that here, for every database, identically.
    Database(std::shared_ptr<Client> mainClient,
             std::vector<std::shared_ptr<Client>> auxiliaryClients,
             std::shared_ptr<spdlog::logger> logger);

    /// @brief Constructor for a deployment with one AQMS database.
    Database(std::shared_ptr<Client> mainClient,
             std::shared_ptr<spdlog::logger> logger);

    /// @name Queries
    /// @{

    /// @brief Fetches every station epoch AQMS knows about.
    /// @result The stations, or why the query could not be answered.
    /// @note One station appears once per epoch - the primary key of
    ///       station_data is (net, sta, ondate) - so a station that moved
    ///       has several entries and all of them come back.
    [[nodiscard]] auto fetchStations() const -> std::expected<std::vector<Station>, QueryError>;

    /// @brief Generates a catalog between now and now - duration.
    /// @note This will change after an action is performed as the event
    ///       state will change.
    /// @note EventSummary and not Event: this is the flattened row a
    ///       catalog needs - one line per event with its preferred origin
    ///       and preferred magnitude - not the whole graph of origins,
    ///       arrivals, and station magnitudes underneath it.
    /// @note A row that cannot be read is skipped rather than failing the
    ///       whole catalog, and logged.
    [[nodiscard]] auto getCatalog(const std::chrono::seconds &duration = std::chrono::weeks {2}) const -> std::expected<std::vector<EventSummary>, QueryError>;

    /// @brief Fetches the alarms for an event from every database.
    /// @note An event picks up alarms on more than one machine over its
    ///       life and the alarm tables are not replicated, so this asks
    ///       the main database and every auxiliary one.  A machine that
    ///       cannot be reached is skipped rather than failing the lot.
    /// @warning Declared but not yet implemented - calling it will not
    ///          link.  The underlying query exists; see
    ///          queries/alarmQueries.hpp.
    [[nodiscard]] auto getAlarms(int64_t eventIdentifier) const -> std::expected<std::vector<AlarmAction>, QueryError>;

    /// @brief Fetches the currently locked events (and who owns the lock).
    /// @result A vector of event identifiers that are locked and the 
    ///         corresponding owner.
    /// @note This changes fairly regularly.
    /// @note The whole table comes back - a handful of analysts means a
    ///       handful of rows.  The frontend intersects it with whatever it
    ///       is showing: locked and mine, indicate it; not mine, ignore it.
    /// @note A lock with no user name recorded comes back as "unknown"
    ///       rather than being dropped - it is still a lock.
    [[nodiscard]] auto getLockedEvents() const -> std::expected<std::vector<EventLock>, QueryError>;

    /// @brief Fetches the waveforms for an event.
    /// @warning Declared but not yet implemented.
    [[nodiscard]] auto fetchWaveforms(int64_t eventIdentifier) const -> std::expected<std::vector<Waveform>, QueryError>;
    /// @brief Fetches the waveforms for an event - for specific streams.
    [[nodiscard]] auto fetchWaveforms(int64_t eventIdentifier, const std::vector<StreamIdentifier> &identifiers) const -> std::expected<std::vector<Waveform>, QueryError>;
    /// @}

    /// @name Actions
    /// @{

    /// @brief Attempts to change the event's status in the database to 
    ///        indicate that the event is unwarranted and "cancel" 
    ///        actions should be taken.
    /// @param[in] The event identifier.
    /// @warning Declared but not yet implemented.
    auto cancel(int64_t eventIdentifier) -> std::expected<void, ActionError>;
    /// @brief Attempts to change the event's status in the database to
    ///        indicate that the event is warranted and "accept"
    ///        actions should be taken.
    /// @param[in] The event identifier.
    /// @warning Declared but not yet implemented.
    auto accept(int64_t eventIdentifier) -> std::expected<void, ActionError>;
    /// @}

    /// @brief Destructor.
    ~Database();

    Database() = delete;
    Database(const Database &) = delete;
    Database(Database &&) noexcept = delete;
    Database& operator=(const Database &) = delete;
    Database& operator=(Database &&) noexcept = delete;
private:
    class DatabaseImpl;
    std::unique_ptr<DatabaseImpl> pImpl; 
};
}
#endif

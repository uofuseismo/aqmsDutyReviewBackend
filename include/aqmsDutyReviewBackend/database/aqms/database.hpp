#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DATABASE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DATABASE_HPP
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace AQMSDutyReviewBackend::Database
{
 class Credentials;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Alarm;
 class Event;
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
    /// @brief Initializes a connection to the database.
    Database(const AQMSDutyReviewBackend::Database::AQMS::Credentials &credentials,
             std::shared_ptr<spdlog::logger> logger);

    /// @name Queries
    /// @{

    /// @brief Generates a catalog between now and now - duration.
    /// @note This will change after an action is performed as the event
    ///       state will change.
    [[nodiscard]] auto getCatalog(const std::chrono::seconds &duration = std::chrono::weeks {2}) const -> std::expected<std::vector<Event>, QueryError>;

    /// @brief Fetches the alarms for this event.
    /// @note This will change after an action is taken since a new set of
    ///       alarms may have been issued.
    [[nodiscard]] auto getAlarms() -> std::expected<std::vector<Alarm>, QueryError>;

    /// @brief Fetches the currently locked events (and who owns the lock).
    /// @result A vector of event identifiers that are locked and the 
    ///         corresponding owner.
    /// @note This changes fairly regularly.
    [[nodiscard]] auto getLockedEvents() const -> std::expected< std::vector<std::pair<int64_t, std::string> >, QueryError>;

    /// @brief Fetches the waveforms for an event.
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
    auto cancel(int64_t eventIdentifier) -> std::expected<void, ActionError>;
    /// @brief Attempts to change the event's status in the database to
    ///        indicate that the event is warranted and "accept"
    ///        actions should be taken.
    /// @param[in] The event identifier.
    auto accept(int64_t eventIdentifier) -> std::expected<void, Actionerror>;
    /// @}

    /// @brief Destructor.
    ~Database();

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

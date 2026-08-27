#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_AQMS_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_AQMS_HPP 
#include <chrono>
#include <memory>
#include <vector>
#include <utility>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database
{
 class Credentials;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Event;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class AQMS aqms.hpp
/// @brief This is a utility to perform some queries against an existing 
///        data source so we can populate the services with live data.
///        This is a read-only database.
/// @note This will be removed.
/// @copyright Ben Baker (University of Utah) distributed under the MIT
///            NO AI license. 
class AQMS
{
public:
    enum class Region
    {
        All,   /*!< Queries all events. */
        Utah,  /*!< Queries events from Utah.  This will include FORGE. */
        YNP,   /*!< Queries events from YNP. */
        FORGE  /*!< Queries events from the FORGE region. */
    };
public:
    /// @brief Constructs an interface to the AQMS library from the given
    ///        postgres credentials.
    AQMS(const AQMSDutyReviewBackend::Database::Credentials &credentials,
         std::shared_ptr<spdlog::logger> logger);
    AQMS(const AQMSDutyReviewBackend::Database::Credentials &credentials,
         const Region region,
         std::shared_ptr<spdlog::logger> logger);

    /// @brief Destructor.
    ~AQMS();

    /// @brief Gets the events starting at the given time until now.
    /// @return The events starting at the given time through now.
    [[nodiscard]] std::vector<Event> getEvents(const std::chrono::nanoseconds &startTime) const;
    /// @brief Gets the events in the given time range.
    /// @param startAndEndTime   startAndEndTime.first is the start time of the
    ///                          query and startAndEndTime.second is the end
    ///                          time.
    /// @throws std::invalid_argument if start and end time is not 
    /// @return The events in the time period.
    [[nodiscard]] std::vector<Event> getEvents(const std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds> &startAndEndTime) const;

    /// @result True indicates the class is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;

    AQMS() = delete;
    AQMS& operator=(const AQMS &) = delete;
    AQMS& operator=(AQMS &&) noexcept = delete;
private:
    class AQMSImpl;
    std::unique_ptr<AQMSImpl> pImpl;
};
}
#endif

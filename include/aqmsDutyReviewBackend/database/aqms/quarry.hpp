#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUARRY_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_QUARRY_HPP
#include <chrono>
#include <memory>
#include <string>
#include <utility>

namespace AQMSDutyReviewBackend::Database::AQMS 
{
/// @class Quarry quarry.hpp
/// @brief Defines a quarry in the AQMS database.  This corresponds
///        to information in the gazetteerpt and gazetteerquarry tables.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Quarry
{
public:
    /// @brief Constructor.
    Quarry();
    /// @brief Copy constructor.
    Quarry(const Quarry &quarry);
    /// @brief Move constructor.
    Quarry(Quarry &&quarry) noexcept;

    /// @brief The quarry name - e.g., BINGHAM.
    /// @param[in] name  The quarry name.
    /// @note Leading and trailing blanks will be removed.
    /// @throws std::invalid_argument if this ends up being empty.
    void setName(const std::string &name);
    /// @result The quarry name.
    /// @throws std::runtime_error if \c hasName() is false.
    [[nodiscard]] std::string getName() const;
    /// @result True indicates the quarry name was set.
    [[nodiscard]] bool hasName() const noexcept;

    /// @brief Sets the quarry's latitude.
    /// @param[in] latitude  The latitude in degrees.
    /// @throws std::invalid_argument if this is not in the range [-90, 90].
    void setLatitude(double latitude);
    /// @brief The latitude in degrees.
    /// @throws std::runtime_error if \c hasLatitude() is false.
    [[nodiscard]] double getLatitude() const;
    /// @result True indicates the latitude was set.
    [[nodiscard]] bool hasLatitude() const noexcept;

    /// @brief Sets the quarry's longitude.
    /// @param[in] longitude   The longitude in degrees.
    /// @note This will be converted to the range [0, 360).
    void setLongitude(double longitude) noexcept; 
    /// @brief The longitude in degrees.
    /// @throws std::runtime_error if \c hasLongitude() is false.
    [[nodiscard]] double getLongitude() const;
    /// @result True indicates the longitude was set.
    [[nodiscard]] bool hasLongitude() const noexcept;

    /// @brief Sets the start and end time of the quarry.
    /// @param[in] startAndEndTime   The start and end time (UTC) of the quarry
    ///                              in seconds since the epoch.
    /// @throws std::invalid_argument if the start time is greater than the
    ///         end time.
    void setStartAndEndTime(const std::pair<std::chrono::seconds, std::chrono::seconds> &startAndEndTime);
    /// @result The start and end time of the quarry.
    /// @throws std::runtime_error if \c hasStartAndEndTime() is false.
    [[nodiscard]] std::pair<std::chrono::seconds, std::chrono::seconds> getStartAndEndTime() const;
    /// @result True indicates the start and end time were set.
    [[nodiscard]] bool hasStartAndEndTime() const noexcept;

    /// @brief Sets when AQMS last modified this row - gazetteerquarry.lddate.
    /// @param[in] loadTime  The load date (UTC) in seconds since the epoch.
    /// @note This is bookkeeping, not science: it is what lets a poller ask
    ///       for only the rows that changed since it last looked, rather
    ///       than re-reading the whole table.  Nothing about the quarry
    ///       itself depends on it.
    void setLoadTime(const std::chrono::seconds &loadTime) noexcept;
    /// @result The load date.
    /// @throws std::runtime_error if \c hasLoadTime() is false.
    [[nodiscard]] std::chrono::seconds getLoadTime() const;
    /// @result True indicates the load date was set.
    [[nodiscard]] bool hasLoadTime() const noexcept;

    /// @brief Destructor.
    ~Quarry();
    /// @brief Copy assignment.
    Quarry& operator=(const Quarry &quarry);
    /// @brief Move assignment.
    Quarry& operator=(Quarry &&quarry) noexcept;
private:
    class QuarryImpl;
    std::unique_ptr<QuarryImpl> pImpl;
};
}
#endif

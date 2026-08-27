#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_LOCAL_MAGNITUDE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_LOCAL_MAGNITUDE_HPP
#include <cstddef>
#include <memory>
#include <vector>
#include <aqmsDutyReviewBackend/database/aqms/magnitude.hpp>
namespace AQMSDutyReviewBackend::Database::AQMS
{   
 class StationLocalMagnitude;
}
namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class LocalMagnitude localMagnitude.hpp
/// @brief Defines a local (Richter) magnitude.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI License.
class LocalMagnitude final : public IMagnitude
{
private:
    using StationLocalMagnitudeType = std::vector<StationLocalMagnitude>;
public:
    using const_iterator = typename StationLocalMagnitudeType::const_iterator;
    /// @note Iteration is read-only, which is how these objects are
    ///       actually used: a query builds the vector, it is moved in
    ///       once with setStationMagnitudes, and from then on the event
    ///       graph is only read - typically straight into JSON.  Nothing
    ///       needs to edit a station magnitude in place, so nothing is
    ///       offered that could.  To change one, take a copy from
    ///       getStationMagnitudes(), edit it, and hand it back.
    using iterator = const_iterator;
public:
    /// @brief Constructor.
    LocalMagnitude();
    /// @brief Copy constructor.
    LocalMagnitude(const LocalMagnitude &magnitude); 
    /// @brief Move constructor.
    LocalMagnitude(LocalMagnitude &&magnitude) noexcept;

    /// @result A type of local magnitude.
    [[nodiscard]] IMagnitude::Type getType() const noexcept final;

    /// @result A deep copy of this local magnitude.
    [[nodiscard]] std::unique_ptr<IMagnitude> clone() const final;

    /// @brief Sets the station magnitudes.
    /// @note The actual local magnitude ends up being something akin
    ///       to an average of these values.
    void setStationMagnitudes(const std::vector<StationLocalMagnitude> &stationMagnitudes);
    void setStationMagnitudes(std::vector<StationLocalMagnitude> &&stationMagnitudes);
    /// @result The station magnitudes.
    [[nodiscard]] std::vector<StationLocalMagnitude> getStationMagnitudes() const;
    /// @result The number of station magnitudes.
    [[nodiscard]] size_t size() const noexcept;

    /// @brief Destructor.
    virtual ~LocalMagnitude();
    /// @brief Copy assignment.
    LocalMagnitude& operator=(const LocalMagnitude &magnitude);
    /// @brief Move assignment.
    LocalMagnitude& operator=(LocalMagnitude &&) noexcept;

    /// @name Read-only access to the station magnitudes
    /// @{
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator cbegin() const;
    [[nodiscard]] const_iterator end() const;
    [[nodiscard]] const_iterator cend() const;
    /// @throws std::out_of_range if pos is not a valid index.
    [[nodiscard]] const StationLocalMagnitude& at(size_t pos) const;
    /// @note Not bounds checked; use at() for an index from outside.
    [[nodiscard]] const StationLocalMagnitude& operator[](size_t pos) const;
    /// @}
private:
    class LocalMagnitudeImpl;
    std::unique_ptr<LocalMagnitudeImpl> pImpl;
};
}
#endif

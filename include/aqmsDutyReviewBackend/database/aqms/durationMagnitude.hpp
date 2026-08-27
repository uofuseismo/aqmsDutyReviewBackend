#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DURATION_MAGNITUDE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DURATION_MAGNITUDE_HPP
#include <cstddef>
#include <memory>
#include <vector>
#include <aqmsDutyReviewBackend/database/aqms/magnitude.hpp>
namespace AQMSDutyReviewBackend::Database::AQMS
{ 
 class StationDurationMagnitude; 
}
namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class DurationMagnitude durationMagnitude.hpp
/// @brief Defines a duration magnitude.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class DurationMagnitude final : public IMagnitude
{
private:
    using StationDurationMagnitudeType = std::vector<StationDurationMagnitude>;
public:
    using iterator = typename StationDurationMagnitudeType::iterator;
    using const_iterator = typename StationDurationMagnitudeType::const_iterator;
public:
    /// @brief Constructor.
    DurationMagnitude();
    /// @brief Copy constructor.
    DurationMagnitude(const DurationMagnitude &magnitude); 
    /// @brief Move constructor.
    DurationMagnitude(DurationMagnitude &&magnitude) noexcept;

    /// @result A type of duration magnitude.
    [[nodiscard]] IMagnitude::Type getType() const noexcept final;

    /// @result A deep copy of this duration magnitude.
    [[nodiscard]] std::unique_ptr<IMagnitude> clone() const final;

    /// @brief Sets the station magnitudes.
    /// @note The actual duration magnitude ends up being an average
    ///       of these values.
    void setStationMagnitudes(const std::vector<StationDurationMagnitude> &stationMagnitudes);
    void setStationMagnitudes(std::vector<StationDurationMagnitude> &&stationMagnitudes);
    /// @result The station magnitudes.
    [[nodiscard]] std::vector<StationDurationMagnitude> getStationMagnitudes() const;
    /// @result The number of station magnitudes.
    [[nodiscard]] size_t size() const noexcept;


    /// @brief Destructor.
    virtual ~DurationMagnitude();
    /// @brief Copy assignment.
    DurationMagnitude& operator=(const DurationMagnitude &magnitude);
    /// @brief Move assignment.
    DurationMagnitude& operator=(DurationMagnitude &&) noexcept;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;
    StationDurationMagnitude& at(size_t pos);
    const StationDurationMagnitude& at(size_t pos) const;
    StationDurationMagnitude& operator[](size_t pos);
    const StationDurationMagnitude& operator[](size_t pos) const;
private:
    class DurationMagnitudeImpl;
    std::unique_ptr<DurationMagnitudeImpl> pImpl;
};
}
#endif

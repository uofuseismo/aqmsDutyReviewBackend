#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SEGMENT_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_SEGMENT_HPP
#include <chrono>
#include <memory>
#include <vector>

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class Segment segment.hpp
/// @brief A waveform is comprised of segments.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Segment
{
public:
    /// @brief Constructor.
    Segment();
    /// @brief Copy constructor.
    Segment(const Segment &segment);
    /// @brief Move constructor.
    Segment(Segment &&segment) noexcept;

    /// @brief Sets the sampling rate for this waveform segment.
    /// @param[in] samplingRate  The sampling rate in Hz.
    /// @throws std::invalid_argument if the sampling rate is not positive.
    void setSamplingRate(double samplingRate);
    /// @result The sampling rate in Hz.
    /// @throws std::runtime_error if \c hasSamplingRate() is false.
    [[nodiscard]] double getSamplingRate() const;
    /// @result True indicates the sampling rate was set.
    [[nodiscard]] bool hasSamplingRate() const noexcept;

    /// @brief Sets the start time (UTC) of segment since the epoch (Jan 1 1970).
    /// @param[in] startTime   The segment's start time. 
    void setStartTime(const std::chrono::nanoseconds startTime) noexcept;
    /// @result The start time (UTC) of the segment since the epoch (Jan 1 1970).
    /// @throws std::runtime_error if \c hasStartTime() is false.
    [[nodiscard]] std::chrono::nanoseconds getStartTime() const; 
    /// @result True indicates the start time was set.
    [[nodiscard]] bool hasStartTime() const noexcept;
 
    /// @brief The samples in this segment.
    /// @param[in] data   The data samples in this waveform's segment.
    /// @note This will be promoted to double precision.
    /// @throws std::invalid_argument if data is empty.
    void setData(std::vector<U> &&data);
    void setData(const std::vector<U> &data);
    /// @result The data samples comprising the waveform's segment.
    /// @throws std::runtime_error if \c hasData() is false.
    [[nodiscard]] std::vector<double> getData() const;
    /// @result True indicates that the data samples were set.
    [[nodiscard]] bool hasData() const noexcept;

    /// @brief Destructor.
    ~Segment();
    /// @brief Copy assignment.
    Segment& operator=(const Segment &segment);
    /// @brief Move assignment.
    Segment& operator=(Segment &&segment) noexcept;
private:
    class SegmentImpl;
    std::unique_ptr<SegmentImpl> pImpl;
};
}
#endif

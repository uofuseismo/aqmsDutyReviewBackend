#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_WAVEFORM_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_WAVEFORM_HPP
#include <chrono>
#include <memory>
#include <vector>
namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Segment;
 class StreamIdentifier;
}
namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class Waveform waveform.hpp
/// @brief A seismic waveform which is comprised of segments.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Waveform
{
private:
    using SegmentType = std::vector<Segment>;
public:
    using iterator = typename SegmentType::iterator;
    using const_iterator = typename SegmentType::const_iterator;
public:
    /// @brief Constructor.
    Waveform();
    /// @brief Copy constructor.
    Waveform(const Waveform &waveform);
    /// @brief Move constructor.
    Waveform(Waveform &&waveform) noexcept;

    /// @brief Sets the stream identifier.
    /// @throws std::invalid_argument if the network, station, channel, or
    ///         location code is not set.
    void setStreamIdentifier(const StreamIdentifier &identifier);
    /// @result The stream identifier.
    /// @throws std::runtime_error if \c hasStreamIdentifier() is false.
    [[nodiscard]] StreamIdentifier getStreamIdentifier() const;
    /// @result True indicates the stream identifier was set.
    [[nodiscard]] bool hasStreamIdentifier() const noexcept;

    /// @brief Sets the waveform segments.
    /// @param[in,out] segments  The waveform segments.
    /// @param[in] merge   This flag indicates the class will try to merge
    ///                    successive waveform segments.
    /// @note The segments will be sorted in increasing order on 
    ///       start time.
    /// @throws std::invalid_argument if any segment
    void setSegments(std::vector<Segment> &&segments, bool merge);
    
    /// @brief Destructor.
    ~Waveform();
    /// @brief Copy assignment.
    Waveform &operator=(const Waveform &waveform);
    /// @brief Move assignment.
    Waveform &operator=(Waveform &&waveform) noexcept;

    iterator begin();
    const_iterator begin() const;
    const_iterator cbegin() const;
    iterator end();
    const_iterator end() const;
    const_iterator cend() const;
    Arrival& at(size_t pos);
    const Arrival& at(size_t pos) const;
    Arrival& operator[](size_t pos);
    const Arrival& operator[](size_t pos) const;
private:
    class WaveformImpl;
    std::unique_ptr<WaveformImpl> pImpl;
};
}
#endif

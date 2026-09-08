#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_STATION_DURATION_MAGNITUDE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_STATION_DURATION_MAGNITUDE_HPP
#include <chrono>
#include <memory>
#include <optional>
namespace AQMSDutyReviewBackend::Database::AQMS
{
 class StreamIdentifier;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class StationDurationMagnitude stationDurationMagnitude.hpp
/// @brief Defines a station duration magnitude.  The station magnitude at 
///        Utah is computed as follows:
///           -2.25 + 2.32*log10(duration) + 0.0023*distance_km + channelMagCorrection
///        and in YNP
///           -2.60 + 2.44 log10(duration) + 0.0040*distance_km + channelMagCorrection
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class StationDurationMagnitude
{
public:
    /// @brief The review status of this observation.
    /// @note Its own status, not the magnitude's.  They differ: the archive
    ///       holds 5,368 automatic codas under reviewed network magnitudes
    ///       and 4 finalized amplitudes under automatic ones, so this says
    ///       something the magnitude's own review status does not.
    enum class ReviewStatus
    {
        Automatic, /*!< Produced by the real-time system. */
        Human,     /*!< A person reviewed it. */
        Finalized  /*!< Reviewed and published. */
    };

    /// @brief Constructor.
    StationDurationMagnitude();
    /// @brief Copy constructor.
    StationDurationMagnitude(const StationDurationMagnitude &magnitude); 
    /// @brief Move constructor.
    StationDurationMagnitude(StationDurationMagnitude &&magnitude) noexcept;

    /// @brief Sets the review status of this observation.
    void setReviewStatus(ReviewStatus status) noexcept;
    /// @result The review status.
    /// @throws std::runtime_error if \c hasReviewStatus() is false.
    [[nodiscard]] ReviewStatus getReviewStatus() const;
    /// @result True indicates the review status was set.
    [[nodiscard]] bool hasReviewStatus() const noexcept;

    /// @brief Sets this channel's magnitude - assoccom.mag.
    /// @param[in] magnitude  The station magnitude.
    /// @note AQMS computes and stores this on EVERY coda, reviewed or not
    ///       - 140,879 of 140,879 automatic rows carry one - so it is read
    ///       rather than recomputed.  Reference implementations of the
    ///       scales themselves live in attic/magnitudeCalculations and are
    ///       not built - reconstructing what the database already stores
    ///       would only be a chance to disagree with it.
    void setMagnitude(double magnitude) noexcept;
    /// @result This channel's magnitude.
    /// @throws std::runtime_error if \c hasMagnitude() is false.
    [[nodiscard]] double getMagnitude() const;
    /// @result True indicates the magnitude was set.
    [[nodiscard]] bool hasMagnitude() const noexcept;

    /// @brief The coda duration (tau in the database).
    /// @param[in] duration  The duration in seconds.
    /// @throws std::invalid_argument if the duration is not positive.
    void setDuration(double duration);
    /// @result The duration in seconds.
    /// @throws std::runtime_error if \c hasDuration() is false.
    [[nodiscard]] double getDuration() const;
    /// @result True indicates the duration was set.
    [[nodiscard]] bool hasDuration() const noexcept;

    /// @brief Sets the magnitude correction.
    /// @param[in] correction  The corretion to add to the magnitude.
    void setCorrection(double correction) noexcept;
    /// @result The station correction.  By default this is 0 since these
    ///         are not used at UUSS. 
    [[nodiscard]] double getCorrection() const noexcept;

    /// @brief Sets the residual magnitude - this channel's magnitude less
    ///        the network magnitude.
    /// @note Always the subtraction, never AQMS's stored magres.  AQMS
///       writes magres only on review, so reading it would give a residual
///       on 4% of what an analyst opens; and the subtraction IS magres -
///       they agree exactly on all 10,485 reviewed rows in the archive - so
///       computing it unconditionally costs nothing and gives one
///       definition instead of two.
    void setResidual(double residual) noexcept;
    /// @result The residual magnitude.
    [[nodiscard]] double getResidual() const;
    /// @result True indicates the residual was set.
    [[nodiscard]] bool hasResidual() const noexcept;

    /// @brief Sets the weight.  This is typically binary 0 or 1.
    /// @param[in] weight  The weight where 0 is disabled and 1 fully utilized.
    /// @throws std::invalid_argument if this is not in the range of [0, 1].
    void setWeight(double weight);
    /// @result The weight.
    /// @throws std::runtime_error if \c hasWeight() is false.
    [[nodiscard]] double getWeight() const;
    /// @result True indicates the weight was set.
    [[nodiscard]] bool hasWeight() const noexcept;

    /// @brief Sets the start of the window the duration was measured
    ///        over.
    /// @param[in] startTime  The window start, in nanoseconds since the
    ///                       epoch, UTC - as every other time in these
    ///                       models is held.
    /// @note The window runs from here to here plus getDuration(); the
    ///       end is not stored separately.
    /// @note getDuration() is in SECONDS while this is in NANOSECONDS, so
    ///       the end is
    ///       getStartTime() + std::chrono::duration_cast
    ///           <std::chrono::nanoseconds>
    ///           (std::chrono::duration<double> {getDuration()}) -
    ///       not the two added as they stand.
    void setStartTime(const std::chrono::nanoseconds &startTime) noexcept;
    /// @result The start of the measurement window.
    /// @throws std::runtime_error if \c hasStartTime() is false.
    [[nodiscard]] std::chrono::nanoseconds getStartTime() const;
    /// @result True indicates the start time was set.
    [[nodiscard]] bool hasStartTime() const noexcept;

    /// @brief Sets the source-receiver distance in meters.
    /// @param[in] distance  The distance in meters.
    /// @throws std::invalid_argument if the distance is negative.
    /// @note Meters, like every other distance in these models - depth
    ///       included.  AQMS stores kilometres and the readers multiply on
    ///       the way in, so the conversion happens once, at the edge.
    ///       Mind the magnitude formula in this class's description, which
    ///       is written in KILOMETRES.
    void setSourceReceiverDistance(double distance);
    /// @result The source-receiver distance in meters, if it was set.
    [[nodiscard]] std::optional<double>
        getSourceReceiverDistance() const noexcept;

    /// @brief Sets the source-to-receiver azimuth.
    /// @param[in] azimuth  The azimuth from the source to the receiver, in
    ///                     degrees, measured clockwise from north.
    /// @throws std::invalid_argument if the azimuth is outside [0,360].
    /// @note Closed at both ends: 0 and 360 name the same direction and
    ///       AQMS may write either.
    void setSourceReceiverAzimuth(double azimuth);
    /// @result The source-to-receiver azimuth in degrees, if it was set.
    [[nodiscard]] std::optional<double>
        getSourceReceiverAzimuth() const noexcept;

    /// @brief Sets the stream the duration was measured on.
    /// @throws std::invalid_argument if the identifier is not complete
    ///         enough to name a stream.
    /// @note A station magnitude belongs to a CHANNEL, not to a station -
    ///       the same station can contribute one per component - so this
    ///       is what tells two of them apart.
    void setStreamIdentifier(const StreamIdentifier &identifier);
    /// @brief Sets the stream the duration was measured on.
    void setStreamIdentifier(StreamIdentifier &&identifier);
    /// @result The stream the duration was measured on.
    /// @throws std::runtime_error if \c hasStreamIdentifier() is false.
    [[nodiscard]] StreamIdentifier getStreamIdentifier() const;
    /// @result True indicates the stream identifier was set.
    [[nodiscard]] bool hasStreamIdentifier() const noexcept;

    /// @brief Destructor.
    ~StationDurationMagnitude();
    /// @brief Copy assignment.
    StationDurationMagnitude& operator=(const StationDurationMagnitude &magnitude);
    /// @brief Move assignment.
    StationDurationMagnitude& operator=(StationDurationMagnitude &&) noexcept;
private:
    class StationDurationMagnitudeImpl;
    std::unique_ptr<StationDurationMagnitudeImpl> pImpl;
};
}
#endif

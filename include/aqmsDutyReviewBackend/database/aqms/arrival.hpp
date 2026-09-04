#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_ARRIVAL_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_ARRIVAL_HPP
#include <cstdint>
#include <chrono>
#include <memory>
#include <optional>

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class StreamIdentifier;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief A arrival in the AQMS database.  This is slightly merged with the
///        assocaro table.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Arrival
{
public:
    /// @brief Defines the seismic phase.
    enum class Phase
    {
        P, /*!< P arrival. */
        S  /*!< S arrival. */
    };
    /// @brief Defines the review status.
    enum class ReviewStatus
    {
        Automatic, /*!< This is an automatically generated arrival. */
        Human,     /*!< This is a human-made arrival. */
        Finalized  /*!< It's exceedingly rare but possible the arrival
                         can end up in this state. */
   };
public:
    /// @brief Constructor.
    Arrival();
    /// @brief Copy constructor.
    Arrival(const Arrival &arrival);
    /// @brief Move constructor.
    Arrival(Arrival &&arrival) noexcept;

    /// @brief Sets the arrival identifier.
    void setIdentifier(int64_t identifier);
    /// @brief Gets the arrival identifier.
    /// @throws std::runtime_error if \c hasIdentifier() is false.
    [[nodiscard]] int64_t getIdentifier() const;
    /// @result True indicates the arrival identifier was set.
    [[nodiscard]] bool hasIdentifier() const noexcept;

    /// @brief Sets the arrival time (UTC).
    void setTime(const std::chrono::nanoseconds &time) noexcept;
    /// @result The arrival time.
    /// @throws std::runtime_error if \c hasTime() is false.
    [[nodiscard]] std::chrono::nanoseconds getTime() const;
    /// @result True inidcates the arrival time was set.
    [[nodiscard]] bool hasTime() const noexcept;

    /// @brief Sets the phase.
    void setPhase(Phase phase) noexcept;
    /// @result Gets the phase.
    /// @throws std::runtime_error if \c hasPhase() is false.
    [[nodiscard]] Phase getPhase() const;
    /// @result True indicates the phase was set.
    [[nodiscard]] bool hasPhase() const noexcept;

    /// @brief Sets the review status.
    void setReviewStatus(ReviewStatus status) noexcept;
    /// @result Gets the review status.
    /// @throws std::runtime_error if \c hasReviewStatus() is false.
    [[nodiscard]] ReviewStatus getReviewStatus() const;
    /// @result True indicates the review status was set.
    [[nodiscard]] bool hasReviewStatus() const noexcept;

    /// @brief Sets the stream identifier on which the arrival was made.
    /// @throws std::invalid_argument if the network, station, channel,
    ///         or location code was not set.
    void setStreamIdentifier(const StreamIdentifier &identifier);
    void setStreamIdentifier(StreamIdentifier &&identifier);
    /// @result Gets the stream identifier.
    /// @throws std::runtime_error if \c hasStreamIdentifier() is false.
    [[nodiscard]] StreamIdentifier getStreamIdentifier() const;
    /// @brief True indicates the stream identifier was set.
    [[nodiscard]] bool hasStreamIdentifier() const noexcept;

    /// @brief Sets the "quality" of the arrival - like a weight.
    ///        This will normally be 0 (worst), 0.25, 0.5, 0.75, or 1 (best).
    /// @throws std::invalid_argument if the quality is negative.
    void setQuality(double quality);
    /// @result the quality of the arrival.
    [[nodiscard]] std::optional<double> getQuality() const noexcept;

    /// @brief Sets the time residual (from the assocaro table).
    /// @param[in] residual   Note, the residual is measured in 
    ///                       observed - estimated.
    void setResidual(const std::chrono::nanoseconds &residual);
    /// @result Gets the residual.
    /// @throws std::runtime_error if \c hasResidual() is false.
    [[nodiscard]] std::chrono::nanoseconds getResidual() const; 
    /// @result True indicates the residual was set.
    [[nodiscard]] bool hasResidual() const noexcept;

    /// @brief Sets the source-receiver distance (assocaro.delta).
    /// @param[in] distance  The distance, in whatever units
    ///                      assocaro.delta carries.  Passed through
    ///                      unconverted - unlike depth, which the readers
    ///                      turn from kilometres into metres, this one is
    ///                      not rescaled anywhere.
    /// @throws std::invalid_argument if the distance is negative.
    /// @note Optional.  It is a property of the ASSOCIATION rather than of
    ///       the arrival - the same pick associated to two origins has two
    ///       distances - so an arrival read outside that context has none.
    void setSourceReceiverDistance(double distance);
    /// @result The source-receiver distance, if it was set.
    [[nodiscard]] std::optional<double>
        getSourceReceiverDistance() const noexcept;

    /// @brief Sets the source-receiver azimuth (assocaro.seaz).
    /// @param[in] azimuth  The azimuth from the source to the receiver, in
    ///                     degrees, measured clockwise from north.
    /// @throws std::invalid_argument if the azimuth is outside [0,360].
    /// @note Closed at both ends: 0 and 360 are the same direction and
    ///       AQMS may write either.
    /// @note Optional, and for the same reason as the distance above.
    void setSourceReceiverAzimuth(double azimuth);
    /// @result The source-receiver azimuth in degrees, if it was set.
    [[nodiscard]] std::optional<double>
        getSourceReceiverAzimuth() const noexcept;

    /// @brief Destructor.
    ~Arrival();
    /// @brief Copy assignment.
    Arrival& operator=(const Arrival &arrival);
    /// @brief Move assignment.
    Arrival& operator=(Arrival &&arrival) noexcept;
private:
    class ArrivalImpl;
    std::unique_ptr<ArrivalImpl> pImpl;
};
}
#endif

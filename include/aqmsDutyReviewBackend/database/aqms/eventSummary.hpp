#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_EVENT_SUMMARY_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_EVENT_SUMMARY_HPP
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <aqmsDutyReviewBackend/database/aqms/event.hpp>
#include <aqmsDutyReviewBackend/database/aqms/magnitude.hpp>
#include <aqmsDutyReviewBackend/database/aqms/origin.hpp>

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class EventSummary eventSummary.hpp
/// @brief This is a high-level summary of the events.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class EventSummary
{
public:
    /// @brief Constructor.
    EventSummary();
    /// @brief Copy constructor.
    EventSummary(const EventSummary &event);
    /// @brief Move constructor.
    EventSummary(EventSummary &&event) noexcept;

    /// @brief Sets the event identifier.
    void setIdentifier(int64_t identifier);
    /// @result The event identifier.
    /// @throws std::runtime_error if \c hasIdentifier() is false.
    [[nodiscard]] int64_t getIdentifier() const;
    /// @result True indicates the event identifier was set.
    [[nodiscard]] bool hasIdentifier() const noexcept;

    /// @brief Sets the event type.
    void setEventType(Event::EventType type) noexcept;
    /// @result The event type.
    /// @throws std::runtime_error if \c hasEventType() is false.
    [[nodiscard]] Event::EventType getEventType() const;
    /// @result True indicates the event type was set.
    [[nodiscard]] bool hasEventType() const noexcept;

    /// @brief Sets the event version number.
    /// @throws std::invalid_argument if the version is negative.
    void setVersion(int version);
    /// @result The event version number.
    /// @note By default this is 0.
    [[nodiscard]] int getVersion() const noexcept; 

    /// @brief Sets the latitude in degrees.
    /// @throws std::invalid_argument if this is not in the range [-90,90].
    void setLatitude(double latitude);
    /// @result The latitude in degrees.
    /// @throws std::runtime_error if \c hasLatitude() is false.
    [[nodiscard]] double getLatitude() const;
    /// @result True indicates the latitude was set.
    [[nodiscard]] bool hasLatitude() const noexcept;

    /// @brief Sets the longitude in degrees.
    /// @note This will make a best effort to convert the longitude to [0,360)
    ///       degrees.
    void setLongitude(double longitude);
    /// @result The longitude in degrees.
    /// @throws std::runtime_error if \c hasLongitude() is false.
    [[nodiscard]] double getLongitude() const;
    /// @result True indicates the longitude was set.
    [[nodiscard]] bool hasLongitude() const noexcept;

    /// @brief Sets the depth in meters.
    /// @throws std::invalid_argument if the depth is less than -10000 m or 
    ///         greater than 1000000 m - AQMS's own bounds on origin.depth,
    ///         so anything the database will store this will read.
    void setDepth(double depth);
    /// @result The depth in meters.
    /// @throws std::runtime_error if \c hasDepth() is false.
    [[nodiscard]] double getDepth() const;
    /// @result True indicates the depth was set.
    [[nodiscard]] bool hasDepth() const noexcept;

    /// @brief Sets the origin time (UTC).
    void setTime(const std::chrono::nanoseconds &time);
    /// @result The origin time.
    /// @throws std::runtime_error if \c hasTime() is false.
    [[nodiscard]] std::chrono::nanoseconds getTime() const;
    /// @result True indicates the origin time was set.
    [[nodiscard]] bool hasTime() const noexcept;

    /// @brief The person who computed this origin.
    /// @param[in] credit   The credit goes to this person - e.g., tflynn.
    void setCredit(const std::string &credit);
    /// @result The person who computed the origin.
    [[nodiscard]] std::optional<std::string> getCredit() const noexcept;  

    /// @brief Sets the maximum azimuthal gap in degrees. 
    /// @param[in] gap   A gap is the difference in (source-to-receiver)
    ///                  back-azimuths between successive stations used during
    ///                  location.  This gap is the maximum of those
    ///                  observations.
    /// @throws std::invalid_argument if this is not in the range [0,360].
    /// @note Closed at both ends: an origin located by a single station
    ///       has no second azimuth to close the gap with, so its gap is
    ///       the whole circle.
    void setMaximumAzimuthalGap(double gap);
    /// @result The maximum azimuthal gap in degrees.
    [[nodiscard]] std::optional<double> getMaximumAzimuthalGap() const; 

    /// @brief Sets the weighted RMS error.
    /// @param[in] wrmse   The weighted RMS error.
    /// @throws std::invalid_argument if the RMSe is negative.
    void setWeightedRootMeanSquaredError(double wrmse);
    /// @result  The weighted RMS.
    [[nodiscard]] std::optional<double> getWeightedRootMeanSquaredError() const;

    /// @brief Sets the number of phases used in location and that were not
    ///        discounted by the locator.
    /// @param[in] nDefiningPhases   The number of defining phases.
    /// @throws std::invalid_argument if this is not positive.
    void setNumberOfDefiningPhases(int nDefiningPhases); 
    /// @result The number of phases actually used by the locator.
    [[nodiscard]] std::optional<int> getNumberOfDefiningPhases() const noexcept;

    /// @brief Sets the geographic type.
    void setGeographicType(Origin::GeographicType type) noexcept;
    /// @result The geographic type.
    /// @throws std::runtime_error if \c hasGeographicType() is false.
    [[nodiscard]] Origin::GeographicType getGeographicType() const;
    /// @result True indicates the geographic type was set.
    [[nodiscard]] bool hasGeographicType() const noexcept;
  
    /// @brief Sets the review status.
    void setReviewStatus(Origin::ReviewStatus status) noexcept;
    /// @result Gets the review status.
    /// @throws std::runtime_error if \c hasReviewStatus() is false.
    [[nodiscard]] Origin::ReviewStatus getReviewStatus() const;
    /// @result True indicates the review status was set.
    [[nodiscard]] bool hasReviewStatus() const noexcept;

    /// @brief Sets where/what computed the origin.
    /// @param[in] source   Usually indicates where the origin came from -
    ///                     e.g., machine1 or machine2 but could be
    ///                     Jiggle.
    /// @note This is origin.subsource, which answers "what produced this"
    ///       and is a different question from who is credited with it.
    /// @note Surrounding blanks are trimmed, and a source that is blank or
    ///       all blanks clears it rather than being stored - the column is
    ///       nullable, and an empty string would name a producer while
    ///       naming nothing.
    void setOriginSource(const std::string &source);
    /// @result Where/what computed the origin, or nullopt if the row does
    ///         not say.
    [[nodiscard]] std::optional<std::string> getOriginSource() const noexcept;
 

    /// @brief Sets the preferred magnitude value.
    /// @throws std::invalid_argument if this is greater than 11 - the same
    ///         ceiling IMagnitude enforces, so a summary cannot reject a
    ///         magnitude the magnitude class accepts.
    void setMagnitudeValue(double magnitude);
    /// @result The preferred magnitude value.
    [[nodiscard]] std::optional<double> getMagnitudeValue() const noexcept;

    /// @brief Sets the preferred magnitude type.
    void setMagnitudeType(IMagnitude::Type magnitudeType) noexcept;
    /// @result The preferred magnitude type. 
    [[nodiscard]] std::optional<IMagnitude::Type> getMagnitudeType() const noexcept;

    /// @brief Destructor.
    ~EventSummary();
    /// @brief Copy assignment.
    EventSummary& operator=(const EventSummary &event);
    /// @brief Move assignment.
    EventSummary& operator=(EventSummary &&event) noexcept;
private:
    class EventSummaryImpl;
    std::unique_ptr<EventSummaryImpl> pImpl;
};
}
#endif

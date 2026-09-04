#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_ORIGIN_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_ORIGIN_HPP 
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Arrival;
 class IMagnitude;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class A simplified container for defining the when and where of an event.
/// @brief An origin defines the when and where of a seismic event.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Origin
{
private:
    using ArrivalType = std::vector<Arrival>;
public:
    using const_iterator = typename ArrivalType::const_iterator;
    /// @note Iteration is read-only, which is how these objects are
    ///       actually used: a query builds the vector, it is moved in
    ///       once with setArrivals, and from then on the event graph is
    ///       only read - typically straight into JSON.  Nothing needs to
    ///       edit an arrival in place, so nothing is offered that could.
    ///
    ///       That also keeps setArrivals' work from being undone behind
    ///       its back: it sorts by time and rejects duplicate
    ///       stream-and-phase pairs, and a mutable iterator would let a
    ///       caller move a time or a stream afterwards with neither check
    ///       re-run.  To change an arrival, take a copy from
    ///       getArrivals(), edit it, and hand it back.
    using iterator = const_iterator;
public:
    /// @brief The geographic type.
    enum class GeographicType
    {
        Unknown,    /*!< The geographic type is unknown. */
        Local,      /*<! This is an event within the authoratative boundary. */ 
        Regional,   /*!< This is near'ish the authoratative boundary but not in.  
                         Think like up to 10 epicentral degrees. */
        Teleseismic /*!< This a teleseism - this is poorly defined in that it is further than regional.
                         Think like past 10 epicentral degrees but really more than 30 epicentral degrees. */
    };
    /// @brief Sets the review status.
    enum class ReviewStatus
    {
        Automatic,   /*!< This is fresh from Earthworm and has not been reviewed. */
        Cancelled,   /*!< The event corresponds to something extraneous to the mission like a large quarry blast or teleseism - why is this here and not with the event class?  Who knows?. */
        Human,       /*!< A human has reviewed and published this event and is subject to further review. */
        Finalized,   /*!< This really means the event was published - though it may have been finalized multiple times. */
        Incomplete   /*!< This really means the event was processed but was not published. */
    };
public:
    /// @brief Constructor.
    Origin();
    /// @brief Copy constructor.
    Origin(const Origin &origin);
    /// @brief Move constructor.
    Origin(Origin &&origin) noexcept;

    /// @brief Sets the origin identifier.
    void setIdentifier(int64_t identifier);
    /// @result The origin identifier.
    /// @throws std::runtime_error if \c hasIdentifier() is false.
    [[nodiscard]] int64_t getIdentifier() const;
    /// @result True indicates the origin identifier was set.
    [[nodiscard]] bool hasIdentifier() const noexcept;

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
 
    /// @brief Sets the arrivals.
    void setArrivals(const std::vector<Arrival> &arrivals);
    /// @brief Sets the arrivals.
    void setArrivals(std::vector<Arrival> &&arrivals);
    /// @result The arrivals that define this origin.
    [[nodiscard]] std::vector<Arrival> getArrivals() const noexcept;
    /// @result The number of arrivals.
    [[nodiscard]] size_t size() const noexcept;

    /// @brief Sets the magnitudes computed for this origin.
    /// @note Magnitudes hang off the ORIGIN and not the event - netmag.orid
    ///       is what ties one to the other.  A relocation gets its own
    ///       magnitudes, and the same event can therefore carry several
    ///       values of the same type that belong to different solutions.
    ///       Holding them here keeps a magnitude next to the location it
    ///       was computed from, so nothing has to be paired up afterwards.
    /// @note Every magnitude must have a different type and exactly one
    ///       must be preferred - origin.prefmag names it.
    /// @throws std::invalid_argument if the vector is empty, a magnitude is
    ///         null, two share a type, or the number preferred is not one.
    void setMagnitudes(const std::vector<std::unique_ptr<IMagnitude>> &magnitudes);
    /// @brief Sets the magnitudes computed for this origin.
    void setMagnitudes(std::vector<std::unique_ptr<IMagnitude>> &&magnitudes);
    /// @result Deep copies of the magnitudes, preserving their derived
    ///         types.
    /// @note Prefer \c magnitudes() unless a copy is genuinely wanted.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] std::vector<std::unique_ptr<IMagnitude>> getMagnitudes() const;
    /// @result A read-only view of the magnitudes.  No copying.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] std::span<const std::unique_ptr<IMagnitude>>
        magnitudes() const &;
    std::span<const std::unique_ptr<IMagnitude>> magnitudes() const && = delete;
    /// @result A deep copy of this origin's preferred magnitude.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] std::unique_ptr<IMagnitude> getPreferredMagnitude() const;
    /// @result A reference to this origin's preferred magnitude.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] const IMagnitude &preferredMagnitude() const &;
    const IMagnitude &preferredMagnitude() const && = delete;
    /// @result True indicates the magnitudes were set.
    [[nodiscard]] bool hasMagnitudes() const noexcept;

    /// @brief Sets the geographic type.
    void setGeographicType(GeographicType type) noexcept;
    /// @result The geographic type.
    /// @throws std::runtime_error if \c hasGeographicType() is false.
    [[nodiscard]] GeographicType getGeographicType() const;
    /// @result True indicates the geographic type was set.
    [[nodiscard]] bool hasGeographicType() const noexcept;

    /// @brief Sets the review status.
    void setReviewStatus(ReviewStatus status) noexcept;
    /// @result Gets the review status.
    /// @throws std::runtime_error if \c hasReviewStatus() is false.
    [[nodiscard]] ReviewStatus getReviewStatus() const;
    /// @result True indicates the review status was set.
    [[nodiscard]] bool hasReviewStatus() const noexcept;

    /// @brief Marks this origin as the preferred origin.
    void setIsPreferred() noexcept;;
    /// @brief Marks this origin as not preferred and should be considered
    ///        inferior (for whatever reason) to the preferred origin.
    void setNotPreferred() noexcept; 
    /// @result True indicates this origin is the preferred origin.
    /// @note By default this is true in this application.
    [[nodiscard]] bool isPreferred() const noexcept;

    /// @brief The person who computed this origin.
    /// @param[in] credit   The credit goes to this person - e.g., tflynn.
    /// @note Surrounding blanks are trimmed, and a credit that is blank or
    ///       all blanks clears it rather than being stored.  The column
    ///       this comes from is legitimately empty for an automatic
    ///       location, and an empty string would have getCredit() report
    ///       that somebody computed the origin while naming nobody.
    void setCredit(const std::string &credit);
    /// @result The person who computed the origin, or nullopt if nobody
    ///         is credited.
    [[nodiscard]] std::optional<std::string> getCredit() const noexcept;  

    /// @brief Destructor.
    ~Origin();
    /// @brief Copy constructor. 
    Origin &operator=(const Origin &origin);
    /// @brief Move constructor.
    Origin &operator=(Origin &&origin) noexcept;

    /// @name Read-only access to the arrivals
    /// @{
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator cbegin() const;
    [[nodiscard]] const_iterator end() const;
    [[nodiscard]] const_iterator cend() const;
    /// @throws std::out_of_range if pos is not a valid index.
    [[nodiscard]] const Arrival& at(size_t pos) const;
    /// @note Not bounds checked; use at() for an index from outside.
    [[nodiscard]] const Arrival& operator[](size_t pos) const;
    /// @}
private:
    class OriginImpl;
    std::unique_ptr<OriginImpl> pImpl;
};
}
#endif

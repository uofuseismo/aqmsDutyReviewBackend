#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_EVENT_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_EVENT_HPP 
#include <cstdint>
#include <memory>
#include <span>
#include <vector>
#include <aqmsDutyReviewBackend/database/aqms/origin.hpp>
namespace AQMSDutyReviewBackend::Database::AQMS
{
 class IMagnitude;
 class Origin;
}
namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class Event event.hpp
/// @brief A simplified seismic event (when + where are the origins and
///        how big are the magnitudes).
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class Event
{
public:
    enum class EventType
    {
        Avalanche,      /*!< An avalanche. */
        Collapse,       /*!< A mine collapse event. */
        Earthquake,     /*!< A regular, old earthquake. */
        Explosion,      /*!< Some type of explosion that isn't a quarry blast. */
        Landslide,      /*!< Landslide. */
        MiningInduced,  /*!< Mining induced event. */
        NuclearTest,    /*!< A nuclear test - Utah is known for its sunsets! */
        QuarryBlast,    /*!< A quarry blast. */
        Sonic,          /*!< A sonic event. */
        SubnetTrigger,  /*<! This is an earthworm artifact. */
        Unknown         /*!< Catch all */
    };
public:
    /// @brief Constructor.
    Event(); 
    /// @brief Copy constructor.
    Event(const Event &event);
    /// @brief Move constructor.
    Event(Event &&event) noexcept;

    /// @brief Sets the event identifier.
    void setIdentifier(int64_t identifier);
    /// @result The event identifier.
    /// @throws std::runtime_error if \c hasIdentifier() is false.
    [[nodiscard]] int64_t getIdentifier() const;
    /// @result True indicates the event identifier was set.
    [[nodiscard]] bool hasIdentifier() const noexcept;

    /// @brief Sets the event version number.
    /// @throws std::invalid_argument if the version is negative - AQMS
    ///         counts up from zero as an event is revised, so a negative
    ///         one is a mistake and not an older version.
    void setVersion(int version);
    /// @result The event version number.
    /// @note By default this is 0.
    [[nodiscard]] int getVersion() const noexcept; 

    /// @brief Sets the origins.
    /// @note Every origin must have an identifier, a latitude, a longitude
    ///       and a time, the identifiers must be distinct, and exactly one
    ///       origin must be the preferred one.
    /// @note Depth is NOT among them.  It is nullable in AQMS, so an
    ///       origin can legitimately arrive without one, and refusing it
    ///       here would discard every other origin on the event along with
    ///       it.  Read it through hasDepth().
    void setOrigins(const std::vector<Origin> &origins);
    void setOrigins(std::vector<Origin> &&origins);
    /// @result True indicates that the origins were set.
    [[nodiscard]] bool hasOrigins() const noexcept; 
    /// @brief The preferred origin.
    /// @throws std::runtime_error if \c hasOrigins() is false.
    [[nodiscard]] Origin getPreferredOrigin() const;
    /// @brief Gets deep copies of the origins.
    /// @note Prefer \c origins() unless a copy is genuinely wanted; this
    ///       copies every origin, and each of those copies its arrivals.
    /// @throws std::runtime_error if \c hasOrigins() is false. 
    [[nodiscard]] std::vector<Origin> getOrigins() const; 

    /// @brief Names the event's preferred magnitude - event.prefmag.
    /// @note An identifier and not a magnitude.  Magnitudes belong to
    ///       origins - netmag.orid is what ties one to the other - so the
    ///       event's preferred magnitude is already held by one of its
    ///       origins, and storing a second copy here would be the same
    ///       value in two places with nothing keeping them equal.  This
    ///       records WHICH one; preferredMagnitude() goes and finds it.
    /// @note AQMS keeps event.prefmag separately from origin.prefmag and
    ///       they need not agree: the event's preferred magnitude can sit
    ///       on an origin that is not the preferred origin.
    void setPreferredMagnitudeIdentifier(int64_t identifier);
    /// @result The identifier of the event's preferred magnitude.
    /// @throws std::runtime_error if
    ///         \c hasPreferredMagnitudeIdentifier() is false.
    [[nodiscard]] int64_t getPreferredMagnitudeIdentifier() const;
    /// @result True indicates the preferred magnitude identifier was set.
    [[nodiscard]] bool hasPreferredMagnitudeIdentifier() const noexcept;

    /// @result A deep copy of the event's preferred magnitude.
    /// @throws std::runtime_error if no identifier was set, or if it names
    ///         a magnitude none of this event's origins holds.
    [[nodiscard]] std::unique_ptr<IMagnitude> getPreferredMagnitude() const;
    /// @result True indicates an identifier was set AND one of the origins
    ///         holds a magnitude carrying it.
    [[nodiscard]] bool hasPreferredMagnitude() const noexcept;

    /// @name Zero-copy views
    /// @{
    /// These are the read path.  An event is built once from a query,
    /// moved in, and from then on only read - usually straight into
    /// JSON - so the getters above, which deep-copy the origins and
    /// everything hanging off them, are the wrong tool for that job.  These
    /// hand back a view of the event's own storage instead.
    ///
    /// @warning A view is only valid while the event is alive and its
    ///          contents unchanged.  Calling setOrigins invalidates every
    ///          view previously handed out, exactly as
    ///          it would for a vector.  The rvalue overloads are deleted
    ///          so a view cannot be taken from a temporary event, which
    ///          is the easiest way to get a dangling one.  N.B. a
    ///          'const &' qualifier alone would not do that - a const
    ///          lvalue reference binds to a temporary perfectly happily -
    ///          so the deleted overloads are what carries the guarantee.

    /// @result A view of the origins, in the order they were set.
    /// @throws std::runtime_error if \c hasOrigins() is false.
    [[nodiscard]] std::span<const Origin> origins() const &;
    std::span<const Origin> origins() const && = delete;
    /// @result The preferred origin.
    /// @throws std::runtime_error if \c hasOrigins() is false.
    [[nodiscard]] const Origin &preferredOrigin() const &;
    const Origin &preferredOrigin() const && = delete;
    /// @result The event's preferred magnitude, found among the origins'
    ///         magnitudes.
    /// @throws std::runtime_error if no identifier was set, or if it names
    ///         a magnitude none of this event's origins holds.
    [[nodiscard]] const IMagnitude &preferredMagnitude() const &;
    const IMagnitude &preferredMagnitude() const && = delete;
    /// @result The origin that holds the event's preferred magnitude.
    /// @note The pairing a review screen actually wants: a magnitude means
    ///       little without the location it was computed from, and this is
    ///       the one lookup that would otherwise be written at every call
    ///       site.
    /// @throws std::runtime_error under the same conditions as
    ///         \c preferredMagnitude().
    [[nodiscard]] const Origin &preferredMagnitudeOrigin() const &;
    const Origin &preferredMagnitudeOrigin() const && = delete;
    /// @}

    /// @brief Sets the event type.
    void setEventType(EventType type) noexcept;
    /// @result The event type.
    /// @throws std::runtime_error if \c hasEventType() is false.
    [[nodiscard]] EventType getEventType() const;
    /// @result True indicates the event type was set.
    [[nodiscard]] bool hasEventType() const noexcept;

    /// @brief Destructor.
    ~Event();
    /// @brief Copy assignment.
    Event &operator=(const Event &event);
    /// @brief Move assignment.
    Event &operator=(Event &&event) noexcept;
private:
    class EventImpl;
    std::unique_ptr<EventImpl> pImpl;
};
}
#endif

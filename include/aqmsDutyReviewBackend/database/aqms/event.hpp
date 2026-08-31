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
    /// @note All origins must have required fields (e.g., latitude, longitude,
    ///       depth, time) and one and only one origin must be the preferred origin.
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

    /// @brief Sets the magnitudes.
    /// @note Every magnitude must have a different type and there must be
    ///       one and only one magnitude that is preferred.
    /// @throws std::invalid_argument if a magnitude is null, two magnitudes
    ///         share a type, or the number of preferred magnitudes is not one.
    void setMagnitudes(const std::vector<std::unique_ptr<IMagnitude>> &magnitudes);
    /// @result Deep copies of the magnitudes, preserving their derived types.
    /// @note Prefer \c magnitudes() unless a copy is genuinely wanted;
    ///       this clones every magnitude, and each clone copies its
    ///       station magnitudes.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] std::vector<std::unique_ptr<IMagnitude>> getMagnitudes() const;
    /// @result A deep copy of the preferred magnitude.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] std::unique_ptr<IMagnitude> getPreferredMagnitude() const;
    /// @result True indicates the magnitudes were set.
    [[nodiscard]] bool hasMagnitudes() const noexcept;

    /// @name Zero-copy views
    /// @{
    /// These are the read path.  An event is built once from a query,
    /// moved in, and from then on only read - usually straight into
    /// JSON - so the getters above, which deep-copy the origins and
    /// clone every magnitude, are the wrong tool for that job.  These
    /// hand back a view of the event's own storage instead.
    ///
    /// @warning A view is only valid while the event is alive and its
    ///          contents unchanged.  Calling setOrigins or setMagnitudes
    ///          invalidates every view previously handed out, exactly as
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
    /// @result A view of the magnitudes, in the order they were set.
    /// @note The elements are unique_ptrs, so a magnitude is read through
    ///       two dereferences - e.g. magnitude->getValue() in a range-for.
    ///       No clone happens.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] std::span<const std::unique_ptr<IMagnitude>>
        magnitudes() const &;
    std::span<const std::unique_ptr<IMagnitude>>
        magnitudes() const && = delete;
    /// @result The preferred origin.
    /// @throws std::runtime_error if \c hasOrigins() is false.
    [[nodiscard]] const Origin &preferredOrigin() const &;
    const Origin &preferredOrigin() const && = delete;
    /// @result The preferred magnitude.
    /// @throws std::runtime_error if \c hasMagnitudes() is false.
    [[nodiscard]] const IMagnitude &preferredMagnitude() const &;
    const IMagnitude &preferredMagnitude() const && = delete;
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

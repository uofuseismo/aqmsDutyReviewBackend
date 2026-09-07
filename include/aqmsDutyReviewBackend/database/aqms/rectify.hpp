#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_RECTIFY_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_RECTIFY_HPP
#include <vector>
#include <spdlog/logger.h>

namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Event;
 class Station;
}

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Asks whether any arrival is missing its source-receiver
///        distance or azimuth.
/// @param[in] event  The event to look over.
/// @result True if at least one arrival on at least one origin is missing
///         one of the two.
/// @note Ask this BEFORE fetching stations.  Most events have the values
///       already, and this walks arrivals the caller is holding anyway,
///       whereas the station list is a round trip to the database.  There
///       is nothing to gain by paying for it and then discovering there
///       was nothing to fix.
[[nodiscard]] bool needsArrivalGeometry(const Event &event);

/// @brief Computes the source-receiver distance and azimuth for arrivals
///        that AQMS did not record one for.
///
/// assocaro.delta and assocaro.seaz are nullable, and in practice often
/// null - the same rows whose weight is null.  Both are recoverable: an
/// origin has a position, a station has a position, and the geometry
/// between them is a geodesic calculation.  This does that calculation for
/// the arrivals that need it and leaves the rest alone.
///
/// @param[in,out] event     The event to repair, in place.
/// @param[in] stations      Every station epoch, as fetchStations gives
///                          them.  A station appears once per epoch, and
///                          the epoch covering the origin time is
///                          preferred - a station that moved is in here
///                          more than once with different coordinates.
/// @param[in] logger        Where a station that cannot be found is
///                          recorded.  Borrowed, may be null.
/// @result How many arrivals gained a distance and azimuth.
///
/// @note A LAST DITCH EFFORT, and deliberately not a correction.  A value
///       AQMS supplied is never overwritten, however odd it looks: this
///       application reports what AQMS holds, and a computed number
///       quietly replacing a stored one would make the two disagree with
///       no way to tell which is on screen.  Only an absent value is
///       filled.
///
/// @note Computed on a WGS84 ellipsoid, which is NOT how AQMS computed the
///       values it did store - hypoinverse uses its own calculation - so
///       the two differ slightly for the same geometry and are expected
///       to.  Measured against 400 associations in the archive that carry
///       both: the distances agree to 0.197% on average, and the worst
///       cases run to about 11% in distance and 7 degrees in azimuth,
///       mostly on short paths where a small coordinate difference is a
///       large fraction of a small number.
///
///       Good enough to orient a record section or sort picks by
///       distance.  Not something to treat as authoritative, and not
///       something to compare against a stored value as though a
///       disagreement meant one of them was wrong.
///
/// @note Distance in kilometres and azimuth in degrees clockwise from
///       north, which is what assocaro.delta and assocaro.seaz carry -
///       checked against the archive rather than assumed.
[[nodiscard]] int rectifyArrivalGeometry(
    Event &event,
    const std::vector<Station> &stations,
    spdlog::logger *logger = nullptr);
}
#endif

#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DISTANCE_CORRECTIONS_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_DISTANCE_CORRECTIONS_HPP
namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @brief Which correction table applies.
/// @note UUSS runs two local magnitude scales, and an event gets the one
///       for the region it is in.  They are genuinely different curves,
///       not one curve with an offset - Yellowstone's is lower everywhere
///       and does not even cover the same distance range - so picking the
///       wrong one is a wrong magnitude, not a slightly wrong one.
enum class MagnitudeRegion
{
    Utah,        /*!< The Utah region scale. */
    Yellowstone  /*!< The Yellowstone region scale. */
};

/// @brief Looks up the local magnitude distance correction.
/// @param[in] distance  The source-receiver distance in METERS, as the
///                      models hold it.
/// @param[in] region    Which scale to read.
/// @result The correction to add to the magnitude.
///
/// @note These are BUCKETS, not a curve to interpolate.  Each tabulated
///       distance owns the range from itself up to the next one, so a
///       station at 72 km gets the correction listed at 70 km, not
///       something between 70 and 80.  That is how the tables have always
///       been read, and interpolating them would quietly produce
///       magnitudes that no previous run of this scale would reproduce.
///
/// @note Below the first distance the first correction applies, and at or
///       beyond the last distance the last one does.  The tables stop at
///       600 km for Utah and 180 km for Yellowstone; past that the answer
///       is the edge value rather than an extrapolation or a refusal,
///       because a station that far out contributes little either way.
///
/// @note Never throws.  A negative distance is not possible from the
///       models - the setters refuse one - and every other input lands in
///       some bucket by construction.
[[nodiscard]] double getDistanceCorrection(double distance,
                                           MagnitudeRegion region) noexcept;
}
#endif

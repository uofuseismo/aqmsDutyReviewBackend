#include <map>
#include "aqmsDutyReviewBackend/database/aqms/distanceCorrections.hpp"

namespace
{

/// The tables are in KILOMETRES, exactly as UUSS writes them, so they can
/// be diffed against the source by eye.  getDistanceCorrection converts on
/// the way in; nothing else here needs to know about meters.
///
/// Keyed by the distance the bucket starts at.  std::map because the
/// lookup is "the greatest tabulated distance not greater than this one",
/// which is upper_bound stepped back one - and because a map cannot get
/// out of order the way two parallel arrays can.  The lists these replaced
/// had 70 and 39 entries and nothing but eyesight keeping them paired.

/// The Utah local magnitude distance corrections, 0 to 600 km.
/// @note The distances step by 5 km to 70, then jump straight to 80, then
///       resume 5 km to 100 before going to 10 km.  There is no 75 km
///       entry.  That is how the list arrived and the count of corrections
///       matches, so it is preserved rather than filled in - it just means
///       the 70 km bucket is twice as wide as its neighbours.
const std::map<double, double> utahCorrections
{
    {    0.0,   1.4}, {    5.0,   1.4}, {   10.0,   1.5}, {   15.0,   1.6},
    {   20.0,   1.7}, {   25.0,   1.9}, {   30.0,   2.1}, {   35.0,   2.3},
    {   40.0,   2.4}, {   45.0,   2.5}, {   50.0,   2.6}, {   55.0,   2.7},
    {   60.0,   2.8}, {   65.0,   2.8}, {   70.0,   2.8}, {   80.0,   2.9},
    {   85.0,   2.9}, {   90.0,   3.0}, {   95.0,   3.0}, {  100.0,   3.0},
    {  110.0,   3.1}, {  120.0,   3.1}, {  130.0,   3.2}, {  140.0,   3.2},
    {  150.0,   3.3}, {  160.0,   3.3}, {  170.0,   3.4}, {  180.0,   3.4},
    {  190.0,   3.5}, {  200.0,   3.5}, {  210.0,   3.6}, {  220.0,  3.65},
    {  230.0,   3.7}, {  240.0,   3.7}, {  250.0,   3.8}, {  260.0,   3.8},
    {  270.0,   3.9}, {  280.0,   3.9}, {  290.0,   4.0}, {  300.0,   4.0},
    {  310.0,   4.1}, {  320.0,   4.1}, {  330.0,   4.2}, {  340.0,   4.2},
    {  350.0,   4.3}, {  360.0,   4.3}, {  370.0,   4.3}, {  380.0,   4.4},
    {  390.0,   4.4}, {  400.0,   4.5}, {  410.0,   4.5}, {  420.0,   4.5},
    {  430.0,   4.6}, {  440.0,   4.6}, {  450.0,   4.6}, {  460.0,   4.6},
    {  470.0,   4.7}, {  480.0,   4.7}, {  490.0,   4.7}, {  500.0,   4.7},
    {  510.0,   4.8}, {  520.0,   4.8}, {  530.0,   4.8}, {  540.0,   4.8},
    {  550.0,   4.8}, {  560.0,   4.9}, {  570.0,   4.9}, {  580.0,   4.9},
    {  590.0,   4.9}, {  600.0,   4.9}
};

/// The Yellowstone local magnitude distance corrections, 3 to 180 km.
/// @note Not monotonic: the corrections rise to 3.17 at 80 km, dip to 3.06
///       at 110 km, then rise again.  That dip is in the source and is not
///       a transcription error.
/// @note Starts at 3 km, not 0 - anything closer takes the 3 km value.
const std::map<double, double> yellowstoneCorrections
{
    {    3.0,  0.64}, {    6.0,  0.72}, {    9.0,  0.87}, {   12.0,  1.09},
    {   15.0,  1.33}, {   18.0,  1.55}, {   21.0,  1.75}, {   25.0,  1.94},
    {   30.0,  2.11}, {   35.0,  2.26}, {   40.0,   2.4}, {   45.0,  2.55},
    {   50.0,  2.69}, {   55.0,  2.82}, {   60.0,  2.94}, {   65.0,  3.04},
    {   70.0,  3.11}, {   75.0,  3.15}, {   80.0,  3.17}, {   85.0,  3.16},
    {   90.0,  3.15}, {   95.0,  3.12}, {  100.0,  3.09}, {  105.0,  3.07},
    {  110.0,  3.06}, {  115.0,  3.07}, {  120.0,   3.1}, {  125.0,  3.15},
    {  130.0,  3.22}, {  135.0,  3.29}, {  140.0,  3.37}, {  145.0,  3.44},
    {  150.0,   3.5}, {  155.0,  3.56}, {  160.0,   3.6}, {  165.0,  3.62},
    {  170.0,  3.65}, {  175.0,  3.66}, {  180.0,  3.67}
};

/// @brief Reads a bucket table.
[[nodiscard]] double lookup(const std::map<double, double> &corrections,
                            const double distanceInKilometers) noexcept
{
    // upper_bound is the first bucket starting AFTER this distance, so the
    // one before it is the bucket this distance falls in.
    auto bucket = corrections.upper_bound(distanceInKilometers);
    if (bucket == corrections.begin())
    {
        // Closer than the table's first entry - the first correction.
        return corrections.begin()->second;
    }
    // Stepping back also lands on the final entry for anything at or past
    // the end of the table, which is the wanted behaviour there too.
    --bucket;
    return bucket->second;
}

}

double AQMSDutyReviewBackend::Database::AQMS::getDistanceCorrection(
    const double distance, const MagnitudeRegion region) noexcept
{
    // The models hold meters; the tables are written in kilometres.
    const auto distanceInKilometers = distance*1.e-3;
    if (region == MagnitudeRegion::Yellowstone)
    {
        return ::lookup(::yellowstoneCorrections, distanceInKilometers);
    }
    return ::lookup(::utahCorrections, distanceInKilometers);
}

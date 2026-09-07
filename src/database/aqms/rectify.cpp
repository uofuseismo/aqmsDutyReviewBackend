#include <chrono>
#include <exception>
#include <map>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include "aqmsDutyReviewBackend/database/aqms/rectify.hpp"
#include "aqmsDutyReviewBackend/database/aqms/arrival.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/geodesic.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

/// A station is identified by its network and its name; the channel and
/// location code play no part, because every channel at a station shares
/// the station's position.
using StationKey = std::pair<std::string, std::string>;

/// @brief Indexes the station epochs by network and name.
/// @note A multimap, not a map: station_data's primary key is
///       (net, sta, ondate), so a station that was moved appears more than
///       once with different coordinates, and which one is right depends
///       on when the event was.
[[nodiscard]] std::multimap<StationKey, const Station *>
indexStations(const std::vector<Station> &stations)
{
    std::multimap<StationKey, const Station *> result;
    for (const auto &station : stations)
    {
        if (!station.hasNetwork() || !station.hasName()){continue;}
        // A station with no position cannot answer the question this is
        // here to ask.
        if (!station.hasLatitude() || !station.hasLongitude()){continue;}
        result.emplace(StationKey {station.getNetwork(), station.getName()},
                       &station);
    }
    return result;
}

/// @brief Picks the station epoch that was running at the given time.
/// @result The epoch covering originTime, or - failing that - any epoch
///         for the station, or nullptr if there is none at all.
/// @note Falling back to another epoch is deliberate.  A station that
///       moved has moved by metres, not by degrees, so the wrong epoch
///       still gives an azimuth good enough to lay out a record section -
///       whereas no epoch at all gives nothing.  A station whose epochs do
///       not cover the event is usually a bookkeeping gap rather than a
///       station that did not exist.
[[nodiscard]] const Station *chooseEpoch(
    const std::multimap<StationKey, const Station *> &index,
    const StationKey &key,
    const std::chrono::seconds &originTime)
{
    const auto range = index.equal_range(key);
    if (range.first == range.second){return nullptr;}
    const Station *fallback{nullptr};
    for (auto entry = range.first; entry != range.second; ++entry)
    {
        const auto *station = entry->second;
        if (fallback == nullptr){fallback = station;}
        if (!station->hasStartAndEndTime()){continue;}
        const auto [startTime, endTime] = station->getStartAndEndTime();
        if (originTime >= startTime && originTime <= endTime)
        {
            return station;
        }
    }
    return fallback;
}

}

bool AQMSDutyReviewBackend::Database::AQMS::needsArrivalGeometry(
    const Event &event)
{
    if (!event.hasOrigins()){return false;}
    for (const auto &origin : event.origins())
    {
        for (const auto &arrival : origin.getArrivals())
        {
            if (!arrival.getSourceReceiverDistance().has_value() ||
                !arrival.getSourceReceiverAzimuth().has_value())
            {
                return true;
            }
        }
    }
    return false;
}

/// @note Every log call is guarded.  The logger is documented as
///       optional - the geometry is worth computing whether or not there
///       is anywhere to report on it - and the SPDLOG_LOGGER_ macros
///       dereference what they are given without checking it.
int AQMSDutyReviewBackend::Database::AQMS::rectifyArrivalGeometry(
    Event &event,
    const std::vector<Station> &stations,
    spdlog::logger *logger)
{
    if (!event.hasOrigins()){return 0;}
    const auto index = ::indexStations(stations);
    if (index.empty()){return 0;}

    int nRectified{0};
    // Rebuilt rather than edited in place: an origin hands out its
    // arrivals as copies on purpose, so that setArrivals' ordering and
    // duplicate checks cannot be undone behind its back.
    auto origins = event.getOrigins();
    for (auto &origin : origins)
    {
        // Nothing to measure from.
        if (!origin.hasLatitude() || !origin.hasLongitude()){continue;}
        if (origin.size() == 0){continue;}
        const auto originTime
            = origin.hasTime()
            ? std::chrono::duration_cast<std::chrono::seconds>
                  (origin.getTime())
            : std::chrono::seconds {0};

        auto arrivals = origin.getArrivals();
        auto changed = false;
        for (auto &arrival : arrivals)
        {
            // Never overwrite what AQMS supplied - only fill a gap.
            const auto needsDistance
                = !arrival.getSourceReceiverDistance().has_value();
            const auto needsAzimuth
                = !arrival.getSourceReceiverAzimuth().has_value();
            if (!needsDistance && !needsAzimuth){continue;}
            if (!arrival.hasStreamIdentifier()){continue;}
            const auto streamIdentifier = arrival.getStreamIdentifier();
            if (!streamIdentifier.hasNetwork() ||
                !streamIdentifier.hasStation())
            {
                continue;
            }
            const StationKey key {streamIdentifier.getNetwork(),
                                  streamIdentifier.getStation()};
            const auto *station = ::chooseEpoch(index, key, originTime);
            if (station == nullptr)
            {
                if (logger != nullptr)
                {
                    SPDLOG_LOGGER_DEBUG(logger,
                                        "No station position for {}.{} - "
                                        "leaving its geometry unset",
                                        key.first, key.second);
                }
                continue;
            }
            try
            {
                const Geodesic::DistanceAzimuth geometry{origin, *station};
                if (needsDistance)
                {
                    if (const auto distance = geometry.getDistance(); distance)
                    {
                        arrival.setSourceReceiverDistance(*distance);
                    }
                }
                if (needsAzimuth)
                {
                    if (const auto azimuth = geometry.getAzimuth(); azimuth)
                    {
                        arrival.setSourceReceiverAzimuth(*azimuth);
                    }
                }
                changed = true;
                ++nRectified;
            }
            catch (const std::exception &e)
            {
                // One unmeasurable pick is not worth the other thirteen.
                if (logger != nullptr)
                {
                    SPDLOG_LOGGER_WARN(logger,
                                       "Could not place {}.{} against origin "
                                       "{} because {}",
                                       key.first, key.second,
                                       origin.hasIdentifier()
                                           ? origin.getIdentifier() : 0,
                                       std::string {e.what()});
                }
            }
        }
        if (changed){origin.setArrivals(std::move(arrivals));}
    }
    if (nRectified > 0)
    {
        event.setOrigins(std::move(origins));
        if (logger != nullptr)
        {
            SPDLOG_LOGGER_INFO(logger,
                               "Computed a distance and azimuth for {} "
                               "arrival(s) AQMS left without one",
                               nRectified);
        }
    }
    return nRectified;
}

#ifndef AQMS_DUTY_REVIEW_BACKEND_TESTING_DATABASE_AQMS_FIXTURES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_TESTING_DATABASE_AQMS_FIXTURES_HPP
/// Builders shared by the AQMS model tests.
///
/// These were an anonymous namespace at the top of one 2,700-line file.
/// They are here so the tests could be split by subject without each file
/// growing its own subtly different "valid origin".
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <type_traits>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
#include "aqmsDutyReviewBackend/database/aqms/geodesic.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/database/aqms/arrival.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/localMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/durationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/humanMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/centroidMomentTensorMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/subnetTrigger.hpp"
#include "aqmsDutyReviewBackend/database/aqms/peakToPeakAmplitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationLocalMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationDurationMagnitude.hpp"


namespace AQMSDutyReviewBackendTesting
{
using namespace AQMSDutyReviewBackend::Database::AQMS;


/// @brief Builds a fully-populated stream identifier for reuse in tests.
inline StreamIdentifier makeStreamIdentifier()
{
    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork("UU");
    streamIdentifier.setStation("CTU");
    streamIdentifier.setChannel("HHZ");
    streamIdentifier.setLocationCode("01");
    return streamIdentifier;
}


/// @brief Builds an origin with all fields required by Event::setOrigins.
//NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
inline Origin makeValidOrigin(const double latitude = 40.77,
                       const double longitude = -111.89,
                       const double depth = 5000,
                       const int64_t identifier = 3233)
{
    Origin origin;
    origin.setIdentifier(identifier);
    origin.setLatitude(latitude);
    origin.setLongitude(longitude);
    origin.setDepth(depth);
    origin.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
    return origin;
}

/// @brief Builds a valid magnitude set: a preferred local magnitude and a
///        non-preferred duration magnitude.
inline std::vector<std::unique_ptr<IMagnitude>> makeMagnitudes()
{
    // Identifiers matter now: event.prefmag names a magnitude by magid and
    // Event finds it by walking its origins, so a magnitude with no
    // identifier can never be the event's preferred one.
    auto local = std::make_unique<LocalMagnitude> ();
    local->setIdentifier(11);
    local->setValue(3.4);
    local->setIsPreferred();

    auto duration = std::make_unique<DurationMagnitude> ();
    duration->setIdentifier(12);
    duration->setValue(3.1);
    duration->setNotPreferred();

    std::vector<std::unique_ptr<IMagnitude>> magnitudes;
    magnitudes.push_back(std::move(local));
    magnitudes.push_back(std::move(duration));
    return magnitudes;
}

/// @brief Builds a stream identifier on the given channel of UU.CTU.
inline StreamIdentifier makeStream(const std::string &channel)
{
    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork("UU");
    streamIdentifier.setStation("CTU");
    streamIdentifier.setChannel(channel);
    streamIdentifier.setLocationCode("01");
    return streamIdentifier;
}

/// @brief Builds a fully-populated peak-to-peak amplitude (millimeters).
inline PeakToPeakAmplitude makeAmplitude(const std::string &channel,
                                  const double amplitudeMillimeters)
{
    PeakToPeakAmplitude amplitude;
    amplitude.setStreamIdentifier(makeStream(channel));
    const std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>
        peakTimes{std::chrono::nanoseconds{1}, std::chrono::nanoseconds{2}};
    amplitude.setPeakTimes(peakTimes);
    amplitude.setAmplitude(amplitudeMillimeters,
                           PeakToPeakAmplitude::Units::Millimeters);
    return amplitude;
}

/// @brief Builds one channel's local magnitude with the given weight.
inline StationLocalMagnitude makeStationLocalMagnitude(const double weight,
                                                const double amplitude)
{
    StationLocalMagnitude magnitude;
    magnitude.setAmplitude(amplitude);
    magnitude.setWeight(weight);
    return magnitude;
}

/// @brief Builds a station duration magnitude with the given weight.
inline StationDurationMagnitude makeStationDurationMagnitude(const double weight,
                                                      const double duration)
{
    StationDurationMagnitude magnitude;
    magnitude.setDuration(duration);
    magnitude.setWeight(weight);
    return magnitude;
}

/// @brief Builds a valid arrival on its own channel at the given time.
/// @note A distinct channel per arrival keeps a set of them clear of the
///       duplicate stream-and-phase check in Origin::setArrivals, which is
///       not what these tests are about.
inline Arrival makeArrivalAt(const int64_t identifier,
                      const std::string &channel,
                      const std::chrono::seconds &time,
                      const Arrival::Phase phase = Arrival::Phase::P)
{
    Arrival arrival;
    arrival.setIdentifier(identifier);
    arrival.setPhase(phase);
    arrival.setStreamIdentifier(makeStream(channel));
    arrival.setTime(time);
    arrival.setReviewStatus(Arrival::ReviewStatus::Automatic);
    return arrival;
}

/// @brief An origin holding three arrivals, handed to setArrivals OUT of
///        time order so the ordering the class imposes is visible.
inline Origin makeOriginWithArrivals()
{
    auto origin = makeValidOrigin();
    origin.setArrivals(std::vector<Arrival> {
        makeArrivalAt(3, "HHZ", std::chrono::seconds {300}),
        makeArrivalAt(1, "HHN", std::chrono::seconds {100}),
        makeArrivalAt(2, "HHE", std::chrono::seconds {200})});
    return origin;
}
}
#endif

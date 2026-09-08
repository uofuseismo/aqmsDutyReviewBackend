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
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
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

#include "fixtures.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
using namespace AQMSDutyReviewBackendTesting;
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Magnitude", "Magnitude")
{
    SECTION("Local magnitude defaults")
    {
        const LocalMagnitude magnitude;
        REQUIRE(magnitude.getType() == IMagnitude::Type::Local);
        REQUIRE(magnitude.isPreferred());
        REQUIRE_FALSE(magnitude.hasIdentifier());
        REQUIRE_FALSE(magnitude.hasValue());
        REQUIRE_FALSE(magnitude.hasReviewStatus());
        REQUIRE_THROWS_AS(magnitude.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getValue(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getReviewStatus(), std::runtime_error);
    }
    SECTION("Types")
    {
        REQUIRE(LocalMagnitude{}.getType() == IMagnitude::Type::Local);
        REQUIRE(DurationMagnitude{}.getType() == IMagnitude::Type::Duration);
        REQUIRE(HumanMagnitude{}.getType() == IMagnitude::Type::Human);
    }
    SECTION("Set and get through base interface")
    {
        constexpr int64_t identifier{93};
        constexpr double value{3.4};
        LocalMagnitude magnitude;
        magnitude.setIdentifier(identifier);
        magnitude.setValue(value);
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Human);
        magnitude.setNotPreferred();

        REQUIRE(magnitude.getIdentifier() == identifier);
        REQUIRE(magnitude.getValue() == value);
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
        REQUIRE_FALSE(magnitude.isPreferred());

        magnitude.setIsPreferred();
        REQUIRE(magnitude.isPreferred());
    }
    SECTION("Value bounds")
    {
        LocalMagnitude magnitude;
        REQUIRE_NOTHROW(magnitude.setValue(11.0));
        REQUIRE(magnitude.getValue() == 11.0);
        REQUIRE_NOTHROW(magnitude.setValue(-1.5));   // Small events are negative.
        REQUIRE(magnitude.getValue() == -1.5);
        REQUIRE_THROWS_AS(magnitude.setValue(11.0001), std::invalid_argument);
    }
    SECTION("Human magnitude defaults to human reviewed")
    {
        const HumanMagnitude magnitude;
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
        // Always readable - the default guarantees a status, so no caller
        // has to check before reading one.
        REQUIRE(magnitude.hasReviewStatus());
    }
    SECTION("A human magnitude reports what the database said")
    {
        // An automatic human magnitude is incoherent, and AQMS may still
        // hold one.  This is a view of AQMS, so it comes back as stored.
        HumanMagnitude magnitude;
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Automatic);
        REQUIRE(magnitude.getReviewStatus()
                == IMagnitude::ReviewStatus::Automatic);
    }
    SECTION("Clone preserves derived type through the base pointer")
    {
        LocalMagnitude local;
        local.setIdentifier(3);
        local.setValue(4.2);
        // Hold it as a base pointer, as Event does, then clone.
        const std::unique_ptr<IMagnitude> base
            = std::make_unique<LocalMagnitude> (local);
        const auto cloned = base->clone();
        REQUIRE(cloned != nullptr);
        REQUIRE(cloned.get() != base.get());          // Distinct object.
        REQUIRE(cloned->getType() == IMagnitude::Type::Local);
        REQUIRE(cloned->getIdentifier() == 3);
        REQUIRE(cloned->getValue() == 4.2);
    }
    SECTION("Copy preserves base state and type independently")
    {
        DurationMagnitude magnitude;
        magnitude.setIdentifier(11);
        magnitude.setValue(2.1);
        magnitude.setNotPreferred();

        const DurationMagnitude copy{magnitude};
        magnitude.setValue(3.3);
        REQUIRE(magnitude.getValue() == 3.3);
        REQUIRE(copy.getType() == IMagnitude::Type::Duration);
        REQUIRE(copy.getIdentifier() == 11);
        REQUIRE(copy.getValue() == 2.1);
        REQUIRE_FALSE(copy.isPreferred());
    }
    SECTION("Move preserves base state and type")
    {
        DurationMagnitude toMove;
        toMove.setIdentifier(11);
        toMove.setValue(2.1);
        const DurationMagnitude moved{std::move(toMove)};
        REQUIRE(moved.getType() == IMagnitude::Type::Duration);
        REQUIRE(moved.getIdentifier() == 11);
        REQUIRE(moved.getValue() == 2.1);
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::PeakToPeakAmplitude",
          "PeakToPeakAmplitude")
{
    SECTION("Defaults")
    {
        const PeakToPeakAmplitude amplitude;
        REQUIRE_FALSE(amplitude.hasStreamIdentifier());
        REQUIRE_FALSE(amplitude.hasPeakTimes());
        REQUIRE_FALSE(amplitude.hasAmplitude());
        REQUIRE_THROWS_AS(amplitude.getStreamIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(amplitude.getPeakTimes(), std::runtime_error);
        REQUIRE_THROWS_AS(amplitude.getAmplitude(), std::runtime_error);
    }
    SECTION("Amplitude is normalized to millimeters")
    {
        PeakToPeakAmplitude amplitude;
        amplitude.setAmplitude(2.0, PeakToPeakAmplitude::Units::Meters);
        REQUIRE(amplitude.getAmplitude() == Catch::Approx(2000.0));
        amplitude.setAmplitude(2.0, PeakToPeakAmplitude::Units::Centimeters);
        REQUIRE(amplitude.getAmplitude() == Catch::Approx(20.0));
        amplitude.setAmplitude(2.0, PeakToPeakAmplitude::Units::Millimeters);
        REQUIRE(amplitude.getAmplitude() == Catch::Approx(2.0));
    }
    SECTION("Non-positive amplitude throws")
    {
        PeakToPeakAmplitude amplitude;
        REQUIRE_THROWS_AS(
            amplitude.setAmplitude(0.0, PeakToPeakAmplitude::Units::Millimeters),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            amplitude.setAmplitude(-1.0, PeakToPeakAmplitude::Units::Meters),
            std::invalid_argument);
        REQUIRE_FALSE(amplitude.hasAmplitude());
    }
    SECTION("Stream identifier and peak times round trip")
    {
        constexpr std::chrono::nanoseconds firstPeak{100};
        constexpr std::chrono::nanoseconds secondPeak{250};
        PeakToPeakAmplitude amplitude;
        amplitude.setStreamIdentifier(makeStream("HHN"));
        const std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>
            peakTimes{firstPeak, secondPeak};
        amplitude.setPeakTimes(peakTimes);

        REQUIRE(amplitude.hasStreamIdentifier());
        REQUIRE(amplitude.getStreamIdentifier().getChannel() == "HHN");
        REQUIRE(amplitude.hasPeakTimes());
        REQUIRE(amplitude.getPeakTimes().first == firstPeak);
        REQUIRE(amplitude.getPeakTimes().second == secondPeak);
    }
    SECTION("Copy is a deep, independent copy")
    {
        auto amplitude = makeAmplitude("HHN", 6.434);
        const PeakToPeakAmplitude copy{amplitude};
        amplitude.setAmplitude(1.0, PeakToPeakAmplitude::Units::Millimeters);
        REQUIRE(copy.getAmplitude() == Catch::Approx(6.434));
        REQUIRE(copy.getStreamIdentifier().getChannel() == "HHN");
    }
    SECTION("Move")
    {
        auto toMove = makeAmplitude("HHE", 3.180);
        const PeakToPeakAmplitude moved{std::move(toMove)};
        REQUIRE(moved.getAmplitude() == Catch::Approx(3.180));
        REQUIRE(moved.getStreamIdentifier().getChannel() == "HHE");
    }
}


/// One CHANNEL's contribution to a local magnitude.  This used to hold a
/// pair of peak-to-peak amplitudes, modelling one station magnitude from
/// two horizontals; AQMS stores a magnitude per component instead, so that
/// shape was removed rather than kept alongside.
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::StationLocalMagnitude",
          "StationLocalMagnitude")
{
    SECTION("Defaults")
    {
        const StationLocalMagnitude magnitude;
        REQUIRE_FALSE(magnitude.hasMagnitude());
        REQUIRE_FALSE(magnitude.hasResidual());
        REQUIRE_FALSE(magnitude.hasWeight());
        REQUIRE_FALSE(magnitude.hasStreamIdentifier());
        REQUIRE_FALSE(magnitude.getAmplitude().has_value());
        REQUIRE_FALSE(magnitude.getSourceReceiverDistance().has_value());
        REQUIRE_FALSE(magnitude.getSourceReceiverAzimuth().has_value());
        REQUIRE(magnitude.getCorrection() == 0.0);
        REQUIRE_THROWS_AS(magnitude.getMagnitude(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getResidual(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getStreamIdentifier(),
                          std::runtime_error);
    }
    SECTION("A channel's magnitude and residual round trip")
    {
        StationLocalMagnitude magnitude;
        magnitude.setMagnitude(0.97);
        magnitude.setResidual(0.97 - 0.79);
        REQUIRE(magnitude.getMagnitude() == 0.97);
        REQUIRE(magnitude.getResidual() == Catch::Approx(0.18));
    }
    SECTION("The amplitude is positive millimetres")
    {
        StationLocalMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setAmplitude(0.0),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setAmplitude(-1.0),
                          std::invalid_argument);
        magnitude.setAmplitude(0.748);
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*magnitude.getAmplitude() == Catch::Approx(0.748));
    }
    SECTION("It is a CHANNEL, so a partial stream is refused")
    {
        StationLocalMagnitude magnitude;
        StreamIdentifier partial;
        partial.setNetwork("UU");
        partial.setStation("FOR5");
        REQUIRE_THROWS_AS(magnitude.setStreamIdentifier(partial),
                          std::invalid_argument);
        partial.setChannel("HHE");
        partial.setLocationCode("01");
        magnitude.setStreamIdentifier(partial);
        REQUIRE(magnitude.getStreamIdentifier().getChannel() == "HHE");
    }
    SECTION("The geometry is optional and bounded")
    {
        StationLocalMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setSourceReceiverDistance(-1.0),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setSourceReceiverAzimuth(360.1),
                          std::invalid_argument);
        magnitude.setSourceReceiverDistance(12'500.0);
        magnitude.setSourceReceiverAzimuth(145.0);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*magnitude.getSourceReceiverDistance() == 12'500.0);
        REQUIRE(*magnitude.getSourceReceiverAzimuth() == 145.0);
        //NOLINTEND(bugprone-unchecked-optional-access)
    }
    SECTION("Weight bounds")
    {
        StationLocalMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setWeight(-0.1), std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setWeight(1.1), std::invalid_argument);
        magnitude.setWeight(1.0);
        REQUIRE(magnitude.getWeight() == 1.0);
    }
    SECTION("Copy is a deep, independent copy")
    {
        StreamIdentifier streamIdentifier;
        streamIdentifier.setNetwork("UU");
        streamIdentifier.setStation("FOR5");
        streamIdentifier.setChannel("HHE");
        streamIdentifier.setLocationCode("01");

        StationLocalMagnitude magnitude;
        magnitude.setStreamIdentifier(streamIdentifier);
        magnitude.setMagnitude(0.97);
        magnitude.setResidual(0.18);
        magnitude.setAmplitude(0.748);
        magnitude.setSourceReceiverDistance(12'500.0);

        const StationLocalMagnitude copy{magnitude};
        magnitude.setMagnitude(2.0);
        REQUIRE(copy.getMagnitude() == 0.97);
        REQUIRE(copy.getResidual() == Catch::Approx(0.18));
        REQUIRE(copy.getStreamIdentifier().getChannel() == "HHE");
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*copy.getAmplitude() == Catch::Approx(0.748));
        REQUIRE(*copy.getSourceReceiverDistance() == 12'500.0);
        //NOLINTEND(bugprone-unchecked-optional-access)
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::StationDurationMagnitude",
          "StationDurationMagnitude")
{
    SECTION("Defaults")
    {
        const StationDurationMagnitude magnitude;
        REQUIRE_FALSE(magnitude.hasDuration());
        REQUIRE_FALSE(magnitude.getSourceReceiverDistance().has_value());
        REQUIRE_FALSE(magnitude.getSourceReceiverAzimuth().has_value());
        REQUIRE_FALSE(magnitude.hasStartTime());
        REQUIRE_FALSE(magnitude.hasStreamIdentifier());
        REQUIRE_FALSE(magnitude.hasResidual());
        REQUIRE_FALSE(magnitude.hasWeight());
        REQUIRE(magnitude.getCorrection() == 0.0);   // Default, not "has".
        REQUIRE_THROWS_AS(magnitude.getStartTime(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getStreamIdentifier(),
                          std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getDuration(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getResidual(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getWeight(), std::runtime_error);
    }
    SECTION("Duration must be positive")
    {
        StationDurationMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setDuration(0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setDuration(-1.0), std::invalid_argument);
        magnitude.setDuration(12.5);
        REQUIRE(magnitude.hasDuration());
        REQUIRE(magnitude.getDuration() == 12.5);
    }
    SECTION("Distance cannot be negative")
    {
        // Meters, and named the way Arrival names it - there is one
        // source-receiver distance on these models, not two.
        StationDurationMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setSourceReceiverDistance(-1.0),
                          std::invalid_argument);
        magnitude.setSourceReceiverDistance(177000.0);
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*magnitude.getSourceReceiverDistance() == 177000.0);
    }
    SECTION("The azimuth is closed at both ends")
    {
        StationDurationMagnitude magnitude;
        REQUIRE_NOTHROW(magnitude.setSourceReceiverAzimuth(0));
        REQUIRE_NOTHROW(magnitude.setSourceReceiverAzimuth(360));
        REQUIRE_THROWS_AS(magnitude.setSourceReceiverAzimuth(-0.1),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setSourceReceiverAzimuth(360.1),
                          std::invalid_argument);
    }
    SECTION("The measurement window start round trips")
    {
        StationDurationMagnitude magnitude;
        magnitude.setStartTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
        REQUIRE(magnitude.hasStartTime());
        REQUIRE(magnitude.getStartTime()
                == std::chrono::nanoseconds{1'700'000'000'000'000'000});
    }
    SECTION("A station magnitude belongs to a channel")
    {
        // Partial streams are refused for the same reason Arrival refuses
        // them: the same station contributes one of these per component,
        // so a stream without a channel cannot tell two of them apart.
        StationDurationMagnitude magnitude;
        StreamIdentifier partial;
        partial.setNetwork("UU");
        partial.setStation("CTU");
        REQUIRE_THROWS_AS(magnitude.setStreamIdentifier(partial),
                          std::invalid_argument);
        REQUIRE_FALSE(magnitude.hasStreamIdentifier());

        partial.setChannel("EHZ");
        partial.setLocationCode("01");
        magnitude.setStreamIdentifier(partial);
        REQUIRE(magnitude.hasStreamIdentifier());
        REQUIRE(magnitude.getStreamIdentifier().getStation() == "CTU");
    }
    SECTION("Correction and residual")
    {
        StationDurationMagnitude magnitude;
        magnitude.setCorrection(-0.55);
        REQUIRE(magnitude.getCorrection() == -0.55);
        magnitude.setResidual(0.22);
        REQUIRE(magnitude.hasResidual());
        REQUIRE(magnitude.getResidual() == 0.22);
    }
    SECTION("Weight bounds")
    {
        StationDurationMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setWeight(-0.1), std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setWeight(1.1), std::invalid_argument);
        REQUIRE_NOTHROW(magnitude.setWeight(1.0));
        REQUIRE(magnitude.getWeight() == 1.0);
    }
    SECTION("Copy is a deep, independent copy")
    {
        StationDurationMagnitude magnitude;
        magnitude.setDuration(12.5);
        magnitude.setSourceReceiverDistance(177000.0);
        magnitude.setSourceReceiverAzimuth(145.0);
        magnitude.setStartTime(std::chrono::nanoseconds{42});

        const StationDurationMagnitude copy{magnitude};
        magnitude.setDuration(20.0);
        REQUIRE(magnitude.getDuration() == 20.0);
        REQUIRE(copy.getDuration() == 12.5);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*copy.getSourceReceiverDistance() == 177000.0);
        REQUIRE(*copy.getSourceReceiverAzimuth() == 145.0);
        //NOLINTEND(bugprone-unchecked-optional-access)
        REQUIRE(copy.getStartTime() == std::chrono::nanoseconds{42});
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::NetworkMagnitudeStations",
          "NetworkMagnitudeStations")
{
    SECTION("LocalMagnitude aggregates station magnitudes")
    {
        StationLocalMagnitude station;
        station.setMagnitude(3.23);
        station.setAmplitude(6.434);
        station.setWeight(1.0);

        LocalMagnitude magnitude;
        magnitude.setValue(3.23);
        magnitude.setStationMagnitudes(
            std::vector<StationLocalMagnitude> {station, station});
        REQUIRE(magnitude.getStationMagnitudes().size() == 2);
        REQUIRE(magnitude.getStationMagnitudes().at(0).getWeight() == 1.0);
        // The base magnitude value is untouched by the station list.
        REQUIRE(magnitude.getValue() == 3.23);
    }
    SECTION("LocalMagnitude station magnitudes can be moved in")
    {
        StationLocalMagnitude station;
        station.setMagnitude(3.23);
        station.setAmplitude(6.434);
        std::vector<StationLocalMagnitude> stations{station, station, station};

        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::move(stations));
        REQUIRE(magnitude.getStationMagnitudes().size() == 3);
    }
    SECTION("DurationMagnitude aggregates station magnitudes")
    {
        StationDurationMagnitude station;
        station.setDuration(12.5);
        station.setSourceReceiverDistance(177000.0);

        DurationMagnitude magnitude;
        magnitude.setValue(2.9);
        magnitude.setStationMagnitudes(
            std::vector<StationDurationMagnitude> {station, station});
        REQUIRE(magnitude.getStationMagnitudes().size() == 2);
        REQUIRE(magnitude.getStationMagnitudes().at(1).getDuration() == 12.5);
        REQUIRE(magnitude.getValue() == 2.9);
    }
}



TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::LocalMagnitude iterators",
          "LocalMagnitude")
{
    SECTION("A magnitude with no station magnitudes iterates over nothing")
    {
        const LocalMagnitude magnitude;
        REQUIRE(magnitude.size() == 0);
        REQUIRE(magnitude.begin() == magnitude.end());
        REQUIRE(magnitude.cbegin() == magnitude.cend());
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 0);

        int visited{0};
        for ([[maybe_unused]] const auto &station : magnitude){++visited;}
        REQUIRE(visited == 0);
    }
    SECTION("Iteration visits every station magnitude in insertion order")
    {
        // Unlike Origin::setArrivals, setStationMagnitudes neither sorts
        // nor validates - what goes in is what comes back, in order.
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0),
            makeStationLocalMagnitude(0.0, 2.0)});

        REQUIRE(magnitude.size() == 3);
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 3);

        std::vector<double> weights;
        for (const auto &station : magnitude)
        {
            weights.push_back(station.getWeight());
        }
        REQUIRE(weights == std::vector<double> {1.0, 0.5, 0.0});
    }
    SECTION("size, distance and getStationMagnitudes agree")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        const auto stations = magnitude.getStationMagnitudes();
        REQUIRE(stations.size() == magnitude.size());
        REQUIRE(static_cast<std::size_t>
                (std::distance(magnitude.begin(), magnitude.end()))
                == magnitude.size());
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            REQUIRE(stations.at(i).getWeight() == magnitude.at(i).getWeight());
        }
    }
    SECTION("at, operator[] and the iterator name the same object")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            const auto offset = static_cast<std::ptrdiff_t> (i);
            REQUIRE(&magnitude.at(i) == &magnitude[i]);
            REQUIRE(&magnitude.at(i)
                    == &*std::next(magnitude.begin(), offset));
        }
    }
    SECTION("at is bounds checked")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0)});
        REQUIRE_THROWS_AS(magnitude.at(1), std::out_of_range);

        const LocalMagnitude empty;
        REQUIRE_THROWS_AS(empty.at(0), std::out_of_range);
    }
    SECTION("Access is read-only even through a non-const magnitude")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0)});
        static_assert(std::is_same_v<LocalMagnitude::iterator,
                                     LocalMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(magnitude.begin()),
                                     LocalMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(*magnitude.begin()),
                                     const StationLocalMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude.at(0)),
                                     const StationLocalMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude[0]),
                                     const StationLocalMagnitude &>);
        REQUIRE(magnitude.begin()->getWeight() == 1.0);
    }
    SECTION("Standard algorithms work over the range")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0),
            makeStationLocalMagnitude(0.0, 2.0)});
        REQUIRE(std::count_if(magnitude.begin(), magnitude.end(),
                              [](const StationLocalMagnitude &station)
                              {
                                  return station.getWeight() > 0.0;
                              }) == 2);
        const auto found
            = std::find_if(magnitude.begin(), magnitude.end(),
                           [](const StationLocalMagnitude &station)
                           {
                               return station.getWeight() == 0.5;
                           });
        REQUIRE(found != magnitude.end());
        REQUIRE(std::distance(magnitude.begin(), found) == 1);
    }
    SECTION("A copy iterates over its own station magnitudes")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        LocalMagnitude copy{magnitude};
        REQUIRE(copy.size() == magnitude.size());
        REQUIRE(&*copy.begin() != &*magnitude.begin());

        copy.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(0.25, 1.0)});
        REQUIRE(copy.size() == 1);
        REQUIRE(magnitude.size() == 2);
        REQUIRE(magnitude.begin()->getWeight() == 1.0);
    }
    SECTION("The station magnitudes survive a clone through the base")
    {
        // clone() returns unique_ptr<IMagnitude>, which has no iterators;
        // this checks the payload is carried across all the same.
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        const auto cloned = magnitude.clone();
        REQUIRE(cloned != nullptr);
        const auto *asLocal
            = dynamic_cast<const LocalMagnitude *> (cloned.get());
        REQUIRE(asLocal != nullptr);
        REQUIRE(asLocal->size() == 2);
        REQUIRE(std::distance(asLocal->begin(), asLocal->end()) == 2);
        REQUIRE(asLocal->begin()->getWeight() == 1.0);
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::DurationMagnitude iterators",
          "DurationMagnitude")
{
    SECTION("A magnitude with no station magnitudes iterates over nothing")
    {
        const DurationMagnitude magnitude;
        REQUIRE(magnitude.size() == 0);
        REQUIRE(magnitude.begin() == magnitude.end());
        REQUIRE(magnitude.cbegin() == magnitude.cend());
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 0);

        int visited{0};
        for ([[maybe_unused]] const auto &station : magnitude){++visited;}
        REQUIRE(visited == 0);
    }
    SECTION("Iteration visits every station magnitude in insertion order")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0),
            makeStationDurationMagnitude(0.0, 10.0)});

        REQUIRE(magnitude.size() == 3);
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 3);

        std::vector<double> durations;
        for (const auto &station : magnitude)
        {
            durations.push_back(station.getDuration());
        }
        REQUIRE(durations == std::vector<double> {30.0, 20.0, 10.0});
    }
    SECTION("size, distance and getStationMagnitudes agree")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        const auto stations = magnitude.getStationMagnitudes();
        REQUIRE(stations.size() == magnitude.size());
        REQUIRE(static_cast<std::size_t>
                (std::distance(magnitude.begin(), magnitude.end()))
                == magnitude.size());
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            REQUIRE(stations.at(i).getDuration()
                    == magnitude.at(i).getDuration());
        }
    }
    SECTION("at, operator[] and the iterator name the same object")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            const auto offset = static_cast<std::ptrdiff_t> (i);
            REQUIRE(&magnitude.at(i) == &magnitude[i]);
            REQUIRE(&magnitude.at(i)
                    == &*std::next(magnitude.begin(), offset));
        }
    }
    SECTION("at is bounds checked")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0)});
        REQUIRE_THROWS_AS(magnitude.at(1), std::out_of_range);

        const DurationMagnitude empty;
        REQUIRE_THROWS_AS(empty.at(0), std::out_of_range);
    }
    SECTION("Access is read-only even through a non-const magnitude")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0)});
        static_assert(std::is_same_v<DurationMagnitude::iterator,
                                     DurationMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(magnitude.begin()),
                                     DurationMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(*magnitude.begin()),
                                     const StationDurationMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude.at(0)),
                                     const StationDurationMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude[0]),
                                     const StationDurationMagnitude &>);
        REQUIRE(magnitude.begin()->getDuration() == 30.0);
    }
    SECTION("Standard algorithms work over the range")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0),
            makeStationDurationMagnitude(0.0, 10.0)});
        REQUIRE(std::count_if(magnitude.begin(), magnitude.end(),
                              [](const StationDurationMagnitude &station)
                              {
                                  return station.getWeight() > 0.0;
                              }) == 2);
        const auto longest
            = std::max_element(magnitude.begin(), magnitude.end(),
                               [](const StationDurationMagnitude &lhs,
                                  const StationDurationMagnitude &rhs)
                               {
                                   return lhs.getDuration()
                                        < rhs.getDuration();
                               });
        REQUIRE(longest == magnitude.begin());
    }
    SECTION("A copy iterates over its own station magnitudes")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        DurationMagnitude copy{magnitude};
        REQUIRE(copy.size() == magnitude.size());
        REQUIRE(&*copy.begin() != &*magnitude.begin());

        copy.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(0.25, 5.0)});
        REQUIRE(copy.size() == 1);
        REQUIRE(magnitude.size() == 2);
        REQUIRE(magnitude.begin()->getDuration() == 30.0);
    }
    SECTION("The station magnitudes survive a clone through the base")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        const auto cloned = magnitude.clone();
        REQUIRE(cloned != nullptr);
        const auto *asDuration
            = dynamic_cast<const DurationMagnitude *> (cloned.get());
        REQUIRE(asDuration != nullptr);
        REQUIRE(asDuration->size() == 2);
        REQUIRE(std::distance(asDuration->begin(), asDuration->end()) == 2);
        REQUIRE(asDuration->begin()->getDuration() == 30.0);
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::CentroidMomentTensorMagnitude",
          "CentroidMomentTensorMagnitude")
{
    // A CMT magnitude is computed out of band from AQMS, so this class
    // carries the punchline and nothing else: no moment tensor, no
    // variance reduction, no measure of waveform fit.  The value just is.
    // Everything here is therefore the base interface - if this test ever
    // needs to reach for something CMT-specific, the class has grown a
    // surface it was deliberately built without.
    SECTION("Defaults")
    {
        const CentroidMomentTensorMagnitude magnitude;
        REQUIRE(magnitude.getType() == IMagnitude::Type::Moment);
        REQUIRE(magnitude.isPreferred());
        REQUIRE_FALSE(magnitude.hasIdentifier());
        REQUIRE_FALSE(magnitude.hasValue());
        REQUIRE_THROWS_AS(magnitude.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getValue(), std::runtime_error);
        // Reviewed out of the box, with nothing set: nobody auto-computes
        // a CMT, so that is the sensible default when a row says nothing.
        REQUIRE(magnitude.hasReviewStatus());
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("The review status defaults to human reviewed")
    {
        // Nobody auto-computes a CMT, so a row that says nothing means a
        // person ran another application, decided the answer was good,
        // and wrote the values in.
        const CentroidMomentTensorMagnitude magnitude;
        REQUIRE(magnitude.hasReviewStatus());
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("The database's answer wins, however odd")
    {
        // An automatic CMT should not exist, but if AQMS holds one this
        // is a view of AQMS and reports it.  The oddity gets logged where
        // the row is read; it does not get laundered here.
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Automatic);
        REQUIRE(magnitude.hasReviewStatus());
        REQUIRE(magnitude.getReviewStatus()
                == IMagnitude::ReviewStatus::Automatic);

        // And back again, so the setter is not one-way.
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Human);
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("The database's answer survives a copy, move and clone")
    {
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setValue(5.7);
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Automatic);

        const CentroidMomentTensorMagnitude copy{magnitude};
        REQUIRE(copy.getReviewStatus() == IMagnitude::ReviewStatus::Automatic);

        const auto cloned = magnitude.clone();
        REQUIRE(cloned->getReviewStatus()
                == IMagnitude::ReviewStatus::Automatic);

        const CentroidMomentTensorMagnitude moved{std::move(magnitude)};
        REQUIRE(moved.getReviewStatus() == IMagnitude::ReviewStatus::Automatic);
    }
    SECTION("Set and get through the base interface")
    {
        constexpr int64_t identifier{451};
        constexpr double value{5.7};
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setIdentifier(identifier);
        magnitude.setValue(value);
        magnitude.setNotPreferred();

        REQUIRE(magnitude.getIdentifier() == identifier);
        REQUIRE(magnitude.getValue() == value);
        REQUIRE(magnitude.hasIdentifier());
        REQUIRE(magnitude.hasValue());
        REQUIRE_FALSE(magnitude.isPreferred());

        magnitude.setIsPreferred();
        REQUIRE(magnitude.isPreferred());
    }
    SECTION("Value bounds are the base class's")
    {
        CentroidMomentTensorMagnitude magnitude;
        REQUIRE_NOTHROW(magnitude.setValue(11.0));
        REQUIRE(magnitude.getValue() == 11.0);
        REQUIRE_NOTHROW(magnitude.setValue(-1.5));
        REQUIRE(magnitude.getValue() == -1.5);
        REQUIRE_THROWS_AS(magnitude.setValue(11.0001), std::invalid_argument);
    }
    SECTION("Clone preserves the derived type through a base pointer")
    {
        CentroidMomentTensorMagnitude moment;
        moment.setIdentifier(12);
        moment.setValue(5.1);
        // Held as a base pointer, which is how Event stores it.
        const std::unique_ptr<IMagnitude> base
            = std::make_unique<CentroidMomentTensorMagnitude> (moment);
        const auto cloned = base->clone();
        REQUIRE(cloned != nullptr);
        REQUIRE(cloned.get() != base.get());
        REQUIRE(cloned->getType() == IMagnitude::Type::Moment);
        REQUIRE(cloned->getIdentifier() == 12);
        REQUIRE(cloned->getValue() == 5.1);
        REQUIRE(cloned->hasReviewStatus());
        REQUIRE(cloned->getReviewStatus() == IMagnitude::ReviewStatus::Human);
        REQUIRE(dynamic_cast<const CentroidMomentTensorMagnitude *>
                (cloned.get()) != nullptr);
    }
    SECTION("Copy preserves base state independently")
    {
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setIdentifier(21);
        magnitude.setValue(4.4);
        magnitude.setNotPreferred();

        const CentroidMomentTensorMagnitude copy{magnitude};
        magnitude.setValue(6.6);
        REQUIRE(magnitude.getValue() == 6.6);
        REQUIRE(copy.getType() == IMagnitude::Type::Moment);
        REQUIRE(copy.getIdentifier() == 21);
        REQUIRE(copy.getValue() == 4.4);
        REQUIRE_FALSE(copy.isPreferred());
    }
    SECTION("Move preserves base state and type")
    {
        CentroidMomentTensorMagnitude toMove;
        toMove.setIdentifier(31);
        toMove.setValue(6.2);
        const CentroidMomentTensorMagnitude moved{std::move(toMove)};
        REQUIRE(moved.getType() == IMagnitude::Type::Moment);
        REQUIRE(moved.getIdentifier() == 31);
        REQUIRE(moved.getValue() == 6.2);
        REQUIRE(moved.hasReviewStatus());
        REQUIRE(moved.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("Moment is its own type alongside the others")
    {
        // Event rejects two magnitudes of the same type, so this being
        // distinct is what lets a CMT sit next to an Ml and an Md.
        REQUIRE(CentroidMomentTensorMagnitude{}.getType()
                != HumanMagnitude{}.getType());
        REQUIRE(CentroidMomentTensorMagnitude{}.getType()
                != LocalMagnitude{}.getType());
        REQUIRE(CentroidMomentTensorMagnitude{}.getType()
                != DurationMagnitude{}.getType());

        auto local = std::make_unique<LocalMagnitude> ();
        local->setValue(3.4);
        local->setNotPreferred();
        auto moment = std::make_unique<CentroidMomentTensorMagnitude> ();
        moment->setValue(5.7);
        moment->setIsPreferred();

        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.push_back(std::move(local));
        magnitudes.push_back(std::move(moment));

        Origin origin;
        REQUIRE_NOTHROW(origin.setMagnitudes(magnitudes));
        REQUIRE(origin.magnitudes().size() == 2);
        REQUIRE(origin.preferredMagnitude().getType()
                == IMagnitude::Type::Moment);
        REQUIRE(origin.preferredMagnitude().getValue() == 5.7);
    }
    SECTION("Two moment magnitudes on one origin are rejected")
    {
        auto first = std::make_unique<CentroidMomentTensorMagnitude> ();
        first->setValue(5.7);
        first->setIsPreferred();
        auto second = std::make_unique<CentroidMomentTensorMagnitude> ();
        second->setValue(5.9);
        second->setNotPreferred();

        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.push_back(std::move(first));
        magnitudes.push_back(std::move(second));

        Origin origin;
        REQUIRE_THROWS_AS(origin.setMagnitudes(magnitudes),
                          std::invalid_argument);
    }
}


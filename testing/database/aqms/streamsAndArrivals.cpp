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
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::StreamIdentifier", "StreamIdentifier")
{
    SECTION("Defaults")
    {
        const StreamIdentifier streamIdentifier;
        REQUIRE_FALSE(streamIdentifier.hasNetwork());
        REQUIRE_FALSE(streamIdentifier.hasStation());
        REQUIRE_FALSE(streamIdentifier.hasChannel());
        REQUIRE_FALSE(streamIdentifier.hasLocationCode());
        REQUIRE_THROWS_AS(streamIdentifier.getNetwork(), std::runtime_error);
        REQUIRE_THROWS_AS(streamIdentifier.getStation(), std::runtime_error);
        REQUIRE_THROWS_AS(streamIdentifier.getChannel(), std::runtime_error);
        REQUIRE_THROWS_AS(streamIdentifier.getLocationCode(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        const auto streamIdentifier = makeStreamIdentifier();
        REQUIRE(streamIdentifier.hasNetwork());
        REQUIRE(streamIdentifier.hasStation());
        REQUIRE(streamIdentifier.hasChannel());
        REQUIRE(streamIdentifier.hasLocationCode());
        REQUIRE(streamIdentifier.getNetwork() == "UU");
        REQUIRE(streamIdentifier.getStation() == "CTU");
        REQUIRE(streamIdentifier.getChannel() == "HHZ");
        REQUIRE(streamIdentifier.getLocationCode() == "01");
    }
    SECTION("Empty codes throw")
    {
        StreamIdentifier streamIdentifier;
        REQUIRE_THROWS_AS(streamIdentifier.setNetwork(""), std::invalid_argument);
        REQUIRE_THROWS_AS(streamIdentifier.setStation(""), std::invalid_argument);
        REQUIRE_THROWS_AS(streamIdentifier.setChannel(""), std::invalid_argument);
    }
    SECTION("Empty location code is valid")
    {
        StreamIdentifier streamIdentifier;
        streamIdentifier.setLocationCode("");
        REQUIRE(streamIdentifier.hasLocationCode());
        REQUIRE(streamIdentifier.getLocationCode().empty());
    }
    SECTION("Copy is a deep, independent copy")
    {
        auto streamIdentifier = makeStreamIdentifier();
        const StreamIdentifier copy{streamIdentifier};
        // Mutating the source must not disturb the copy.
        streamIdentifier.setNetwork("WY");
        REQUIRE(streamIdentifier.getNetwork() == "WY");
        REQUIRE(copy.getNetwork() == "UU");
        REQUIRE(copy.getStation() == "CTU");
        REQUIRE(copy.getChannel() == "HHZ");
        REQUIRE(copy.getLocationCode() == "01");
    }
    SECTION("Move")
    {
        auto toMove = makeStreamIdentifier();
        const StreamIdentifier moved{std::move(toMove)};
        REQUIRE(moved.getNetwork() == "UU");
        REQUIRE(moved.getChannel() == "HHZ");
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Arrival association",
          "Arrival")
{
    // delta and seaz describe the pick's association with one origin, not
    // the pick itself, so they are optional and absent by default.
    SECTION("Absent until set")
    {
        const Arrival arrival;
        REQUIRE_FALSE(arrival.getSourceReceiverDistance().has_value());
        REQUIRE_FALSE(arrival.getSourceReceiverAzimuth().has_value());
    }
    SECTION("They round trip")
    {
        Arrival arrival;
        arrival.setSourceReceiverDistance(12.5);
        arrival.setSourceReceiverAzimuth(145.0);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*arrival.getSourceReceiverDistance() == Catch::Approx(12.5));
        REQUIRE(*arrival.getSourceReceiverAzimuth() == Catch::Approx(145.0));
        //NOLINTEND(bugprone-unchecked-optional-access)
    }
    SECTION("A negative distance is refused")
    {
        Arrival arrival;
        REQUIRE_THROWS_AS(arrival.setSourceReceiverDistance(-1),
                          std::invalid_argument);
        REQUIRE_FALSE(arrival.getSourceReceiverDistance().has_value());
        REQUIRE_NOTHROW(arrival.setSourceReceiverDistance(0));
    }
    SECTION("The azimuth is closed at both ends")
    {
        // 0 and 360 name the same direction and AQMS may write either, so
        // neither is an off-by-one to reject.
        Arrival arrival;
        REQUIRE_NOTHROW(arrival.setSourceReceiverAzimuth(0));
        REQUIRE_NOTHROW(arrival.setSourceReceiverAzimuth(360));
        REQUIRE_THROWS_AS(arrival.setSourceReceiverAzimuth(-0.1),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(arrival.setSourceReceiverAzimuth(360.1),
                          std::invalid_argument);
    }
    SECTION("They survive a copy")
    {
        Arrival arrival;
        arrival.setSourceReceiverDistance(12.5);
        arrival.setSourceReceiverAzimuth(145.0);
        const Arrival copy{arrival};
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*copy.getSourceReceiverDistance() == Catch::Approx(12.5));
        REQUIRE(*copy.getSourceReceiverAzimuth() == Catch::Approx(145.0));
        //NOLINTEND(bugprone-unchecked-optional-access)
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Arrival", "Arrival")
{
    SECTION("Defaults")
    {
        const Arrival arrival;
        REQUIRE_FALSE(arrival.hasIdentifier());
        REQUIRE_FALSE(arrival.hasTime());
        REQUIRE_FALSE(arrival.hasPhase());
        REQUIRE_FALSE(arrival.hasReviewStatus());
        REQUIRE_FALSE(arrival.hasStreamIdentifier());
        REQUIRE_FALSE(arrival.hasResidual());
        REQUIRE(arrival.getQuality() == std::nullopt);
        REQUIRE_THROWS_AS(arrival.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getTime(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getPhase(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getReviewStatus(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getStreamIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getResidual(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        constexpr int64_t identifier{482};
        constexpr std::chrono::nanoseconds time{1'700'000'000'123'456'789};
        constexpr std::chrono::nanoseconds residual{-25'000'000};
        constexpr double quality{0.75};

        Arrival arrival;
        arrival.setIdentifier(identifier);
        arrival.setTime(time);
        arrival.setPhase(Arrival::Phase::S);
        arrival.setReviewStatus(Arrival::ReviewStatus::Human);
        arrival.setStreamIdentifier(makeStreamIdentifier());
        arrival.setQuality(quality);
        arrival.setResidual(residual);

        REQUIRE(arrival.getIdentifier() == identifier);
        REQUIRE(arrival.getTime() == time);
        REQUIRE(arrival.getPhase() == Arrival::Phase::S);
        REQUIRE(arrival.getReviewStatus() == Arrival::ReviewStatus::Human);
        REQUIRE(arrival.hasStreamIdentifier());
        REQUIRE(arrival.getStreamIdentifier().getStation() == "CTU");
        const auto quality2 = arrival.getQuality();
        REQUIRE(quality2.has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*quality2 == quality);
        REQUIRE(arrival.getResidual() == residual);
    }
    SECTION("Negative quality throws")
    {
        Arrival arrival;
        REQUIRE_THROWS_AS(arrival.setQuality(-0.1), std::invalid_argument);
        REQUIRE(arrival.getQuality() == std::nullopt);
    }
    SECTION("Incomplete stream identifier throws")
    {
        StreamIdentifier missingChannel;
        missingChannel.setNetwork("UU");
        missingChannel.setStation("CTU");
        missingChannel.setLocationCode("01");
        Arrival arrival;
        REQUIRE_THROWS_AS(arrival.setStreamIdentifier(missingChannel),
                          std::invalid_argument);
        REQUIRE_FALSE(arrival.hasStreamIdentifier());
    }
    SECTION("Copy is a deep, independent copy")
    {
        Arrival arrival;
        arrival.setIdentifier(7);
        arrival.setPhase(Arrival::Phase::P);
        arrival.setStreamIdentifier(makeStreamIdentifier());

        const Arrival copy{arrival};
        arrival.setIdentifier(8);
        REQUIRE(arrival.getIdentifier() == 8);
        REQUIRE(copy.getIdentifier() == 7);
        REQUIRE(copy.getPhase() == Arrival::Phase::P);
        REQUIRE(copy.getStreamIdentifier().getNetwork() == "UU");
    }
    SECTION("Move")
    {
        Arrival toMove;
        toMove.setIdentifier(7);
        toMove.setStreamIdentifier(makeStreamIdentifier());
        const Arrival moved{std::move(toMove)};
        REQUIRE(moved.getIdentifier() == 7);
        REQUIRE(moved.getStreamIdentifier().getChannel() == "HHZ");
    }
}


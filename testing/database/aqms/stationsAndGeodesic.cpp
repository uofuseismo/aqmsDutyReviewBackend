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
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Station", "Station")
{
    SECTION("Defaults")
    {
        const Station station;
        REQUIRE_FALSE(station.hasNetwork());
        REQUIRE_FALSE(station.hasName());
        REQUIRE_FALSE(station.hasLatitude());
        REQUIRE_FALSE(station.hasLongitude());
        REQUIRE_FALSE(station.hasStartAndEndTime());
        REQUIRE_THROWS_AS(station.getNetwork(), std::runtime_error);
        REQUIRE_THROWS_AS(station.getName(), std::runtime_error);
        REQUIRE_THROWS_AS(station.getLatitude(), std::runtime_error);
        REQUIRE_THROWS_AS(station.getLongitude(), std::runtime_error);
        REQUIRE_THROWS_AS(station.getStartAndEndTime(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        Station station;
        station.setNetwork("UU");
        station.setName("CWU");
        station.setLatitude(40.445);
        station.setLongitude(-111.485);
        const std::pair<std::chrono::seconds, std::chrono::seconds>
            startAndEndTime{std::chrono::seconds {1000},
                            std::chrono::seconds {2000}};
        station.setStartAndEndTime(startAndEndTime);

        REQUIRE(station.hasNetwork());
        REQUIRE(station.hasName());
        REQUIRE(station.getNetwork() == "UU");
        REQUIRE(station.getName() == "CWU");
        REQUIRE(station.getLatitude() == Catch::Approx(40.445));
        REQUIRE(station.getStartAndEndTime() == startAndEndTime);
    }
    SECTION("Codes are stripped of blanks and upper-cased")
    {
        // AQMS stores these in fixed-width columns, so they arrive padded.
        // "UU  " and "UU" are the same network and must not compare
        // unequal downstream.
        Station station;
        station.setNetwork("  uu ");
        station.setName("\tcwu\n");
        REQUIRE(station.getNetwork() == "UU");
        REQUIRE(station.getName() == "CWU");

        Station mixed;
        mixed.setNetwork("Uu");
        mixed.setName("cWu");
        REQUIRE(mixed.getNetwork() == "UU");
        REQUIRE(mixed.getName() == "CWU");
    }
    SECTION("A code that is blank, or only blanks, is rejected")
    {
        Station station;
        REQUIRE_THROWS_AS(station.setNetwork(""), std::invalid_argument);
        REQUIRE_THROWS_AS(station.setNetwork("   "), std::invalid_argument);
        REQUIRE_THROWS_AS(station.setNetwork("\t\n"), std::invalid_argument);
        REQUIRE_THROWS_AS(station.setName(""), std::invalid_argument);
        REQUIRE_THROWS_AS(station.setName("  "), std::invalid_argument);
        // A rejected code leaves the station as it was.
        REQUIRE_FALSE(station.hasNetwork());
        REQUIRE_FALSE(station.hasName());
    }
    SECTION("Latitude bounds")
    {
        Station station;
        REQUIRE_THROWS_AS(station.setLatitude(-90.0001),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(station.setLatitude(90.0001),
                          std::invalid_argument);
        REQUIRE_NOTHROW(station.setLatitude(-90.0));
        REQUIRE(station.getLatitude() == Catch::Approx(-90.0));
        REQUIRE_NOTHROW(station.setLatitude(90.0));
        REQUIRE(station.getLatitude() == Catch::Approx(90.0));
    }
    SECTION("Longitude is normalized to [0, 360)")
    {
        // The same convention Origin uses, so a station and an origin can
        // be compared without one being -111.89 and the other 248.11.
        Station station;
        station.setLongitude(-111.89);
        REQUIRE(station.getLongitude() == Catch::Approx(248.11));

        station.setLongitude(248.11);
        REQUIRE(station.getLongitude() == Catch::Approx(248.11));

        station.setLongitude(360.0);
        REQUIRE(station.getLongitude() == Catch::Approx(0.0));

        station.setLongitude(-720.0 + 45.0);
        REQUIRE(station.getLongitude() == Catch::Approx(45.0));
    }
    SECTION("A station cannot stop before it starts")
    {
        Station station;
        const std::pair<std::chrono::seconds, std::chrono::seconds>
            backwards{std::chrono::seconds {2000},
                      std::chrono::seconds {1000}};
        REQUIRE_THROWS_AS(station.setStartAndEndTime(backwards),
                          std::invalid_argument);
        REQUIRE_FALSE(station.hasStartAndEndTime());

        // A station that started and stopped in the same second is odd but
        // not contradictory.
        const std::pair<std::chrono::seconds, std::chrono::seconds>
            instant{std::chrono::seconds {1000}, std::chrono::seconds {1000}};
        REQUIRE_NOTHROW(station.setStartAndEndTime(instant));
        REQUIRE(station.getStartAndEndTime() == instant);
    }
    SECTION("Copy is a deep, independent copy")
    {
        Station station;
        station.setNetwork("UU");
        station.setName("CWU");
        station.setLatitude(40.445);
        station.setLongitude(-111.485);

        const Station copy{station};
        station.setName("SPU");
        station.setLatitude(41.0);

        REQUIRE(station.getName() == "SPU");
        REQUIRE(copy.getNetwork() == "UU");
        REQUIRE(copy.getName() == "CWU");
        REQUIRE(copy.getLatitude() == Catch::Approx(40.445));
        REQUIRE(copy.getLongitude() == Catch::Approx(248.515));
        REQUIRE_FALSE(copy.hasStartAndEndTime());
    }
    SECTION("Move")
    {
        Station toMove;
        toMove.setNetwork("UU");
        toMove.setName("CWU");
        toMove.setLatitude(40.445);
        const Station moved{std::move(toMove)};
        REQUIRE(moved.getNetwork() == "UU");
        REQUIRE(moved.getName() == "CWU");
        REQUIRE(moved.getLatitude() == Catch::Approx(40.445));
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Geodesic",
          "Geodesic")
{
    SECTION("Defaults")
    {
        const Geodesic::DistanceAzimuth geo;
        REQUIRE(geo.getDistance() == std::nullopt);
        REQUIRE(geo.getAzimuth() == std::nullopt);
        REQUIRE(geo.getBackAzimuth() == std::nullopt);
    }   

    SECTION("Bad arguments")
    {
        Geodesic::DistanceAzimuth geo;
        REQUIRE_THROWS_AS(geo.setDistance(-1),     std::invalid_argument);
        REQUIRE_THROWS_AS(geo.setAzimuth(-1),      std::invalid_argument);
        REQUIRE_THROWS_AS(geo.setAzimuth(360),     std::invalid_argument);
        REQUIRE_THROWS_AS(geo.setBackAzimuth(-1),  std::invalid_argument);
        REQUIRE_THROWS_AS(geo.setBackAzimuth(360), std::invalid_argument);
    }

    SECTION("From origin/station")
    {
        Origin origin;
        origin.setLatitude(19);
        origin.setLongitude(32.5);

        Station station;
        station.setLatitude(-20.0);
        station.setLongitude(272.4);

        const Geodesic::DistanceAzimuth distaz(origin, station);
        REQUIRE_THAT(*distaz.getAzimuth(),
                     Catch::Matchers::WithinAbs(-101.69839545926366+360, 1.e-10));
        REQUIRE_THAT(*distaz.getBackAzimuth(),
                     Catch::Matchers::WithinAbs(-99.84855154050379+180, 1.e-10));
        REQUIRE_THAT(*distaz.getDistance(),
                     Catch::Matchers::WithinAbs(13779.227281877534, 1.e-7));
    }
}

namespace
{




}








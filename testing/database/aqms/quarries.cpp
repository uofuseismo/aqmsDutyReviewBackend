#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aqmsDutyReviewBackend/database/aqms/quarry.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

/// @brief A quarry with every field populated - BINGHAM, near enough.
[[nodiscard]] Quarry makeQuarry()
{
    Quarry quarry;
    quarry.setName("BINGHAM");
    quarry.setLatitude(40.5231);
    quarry.setLongitude(-112.1508);
    quarry.setStartAndEndTime({std::chrono::seconds {1640995200},
                               std::chrono::seconds {32503680000}});
    quarry.setLoadTime(std::chrono::seconds {1678127941});
    return quarry;
}

}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Quarry", "[quarry]")
{
    SECTION("Defaults")
    {
        const Quarry quarry;
        REQUIRE_FALSE(quarry.hasName());
        REQUIRE_FALSE(quarry.hasLatitude());
        REQUIRE_FALSE(quarry.hasLongitude());
        REQUIRE_FALSE(quarry.hasStartAndEndTime());
        REQUIRE_FALSE(quarry.hasLoadTime());
        REQUIRE_THROWS_AS(quarry.getName(), std::runtime_error);
        REQUIRE_THROWS_AS(quarry.getLatitude(), std::runtime_error);
        REQUIRE_THROWS_AS(quarry.getLongitude(), std::runtime_error);
        REQUIRE_THROWS_AS(quarry.getStartAndEndTime(), std::runtime_error);
        REQUIRE_THROWS_AS(quarry.getLoadTime(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        const auto quarry = ::makeQuarry();
        REQUIRE(quarry.hasName());
        REQUIRE(quarry.getName() == "BINGHAM");
        REQUIRE(quarry.getLatitude() == Catch::Approx(40.5231));
        REQUIRE(quarry.getLongitude() == Catch::Approx(360 - 112.1508));
        const auto [onDate, offDate] = quarry.getStartAndEndTime();
        REQUIRE(onDate == std::chrono::seconds {1640995200});
        REQUIRE(offDate == std::chrono::seconds {32503680000});
        REQUIRE(quarry.getLoadTime() == std::chrono::seconds {1678127941});
    }
    SECTION("The name is stripped of surrounding blanks")
    {
        // AQMS stores this in a fixed-width column, so it arrives padded.
        Quarry quarry;
        quarry.setName("  BINGHAM \t\n");
        REQUIRE(quarry.getName() == "BINGHAM");
    }
    SECTION("The name keeps its case")
    {
        // Unlike a network or station code.  The gazetteer really holds
        // "arizona #1 AZ (Old List)", and it is a label a person reads
        // rather than an identifier anything matches on.
        Quarry quarry;
        quarry.setName("arizona #1 AZ (Old List)");
        REQUIRE(quarry.getName() == "arizona #1 AZ (Old List)");
    }
    SECTION("A name that is blank, or only blanks, is rejected")
    {
        Quarry quarry;
        REQUIRE_THROWS_AS(quarry.setName(""), std::invalid_argument);
        REQUIRE_THROWS_AS(quarry.setName("   "), std::invalid_argument);
        REQUIRE_THROWS_AS(quarry.setName("\t\n"), std::invalid_argument);
        // A rejected name leaves the quarry as it was.
        REQUIRE_FALSE(quarry.hasName());
    }
    SECTION("Latitude bounds")
    {
        Quarry quarry;
        REQUIRE_THROWS_AS(quarry.setLatitude(-90.0001),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(quarry.setLatitude(90.0001),
                          std::invalid_argument);
        REQUIRE_NOTHROW(quarry.setLatitude(-90.0));
        REQUIRE(quarry.getLatitude() == Catch::Approx(-90.0));
        REQUIRE_NOTHROW(quarry.setLatitude(90.0));
        REQUIRE(quarry.getLatitude() == Catch::Approx(90.0));
    }
    SECTION("Longitude is normalized to [0, 360)")
    {
        // The same convention Origin and Station use, so a quarry and an
        // origin can be compared without one being -112.15 and the other
        // 247.85 - which is the whole point of holding quarries here: to
        // ask whether an event happened at one.
        Quarry quarry;
        quarry.setLongitude(-112.1508);
        REQUIRE(quarry.getLongitude() == Catch::Approx(247.8492));
        quarry.setLongitude(247.8492);
        REQUIRE(quarry.getLongitude() == Catch::Approx(247.8492));
        quarry.setLongitude(-472.1508);   // Another turn round.
        REQUIRE(quarry.getLongitude() == Catch::Approx(247.8492));
        quarry.setLongitude(360);
        REQUIRE(quarry.getLongitude() == Catch::Approx(0));
    }
    SECTION("An epoch that ends before it starts is rejected")
    {
        Quarry quarry;
        REQUIRE_THROWS_AS(
            quarry.setStartAndEndTime({std::chrono::seconds {2000},
                                       std::chrono::seconds {1000}}),
            std::invalid_argument);
        REQUIRE_FALSE(quarry.hasStartAndEndTime());
        // An instantaneous epoch is not an inverted one.
        REQUIRE_NOTHROW(
            quarry.setStartAndEndTime({std::chrono::seconds {1000},
                                       std::chrono::seconds {1000}}));
        REQUIRE(quarry.hasStartAndEndTime());
    }
    SECTION("Copy and move")
    {
        const auto quarry = ::makeQuarry();

        const auto copy = quarry;
        REQUIRE(copy.getName() == "BINGHAM");
        REQUIRE(copy.getLatitude() == Catch::Approx(40.5231));
        REQUIRE(copy.getLongitude() == Catch::Approx(247.8492));
        REQUIRE(copy.getStartAndEndTime() == quarry.getStartAndEndTime());
        REQUIRE(copy.getLoadTime() == quarry.getLoadTime());

        auto source = ::makeQuarry();
        const auto moved = std::move(source);
        REQUIRE(moved.getName() == "BINGHAM");
        REQUIRE(moved.getLoadTime() == std::chrono::seconds {1678127941});

        // Assignment, not just construction - the copy is deep, so
        // writing to the original must not reach the copy.
        Quarry assigned;
        assigned = quarry;
        REQUIRE(assigned.getName() == "BINGHAM");
        Quarry mutated{quarry};
        mutated.setName("BARNEYS CANYON");
        REQUIRE(quarry.getName() == "BINGHAM");
        REQUIRE(mutated.getName() == "BARNEYS CANYON");
    }
}

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
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::EventSummary",
          "EventSummary")
{
    // The flattened row a catalog query returns: one event, its preferred
    // origin, and its preferred magnitude, without the arrivals or the
    // station magnitudes underneath them.
    SECTION("Defaults")
    {
        const EventSummary summary;
        REQUIRE_FALSE(summary.hasIdentifier());
        REQUIRE_FALSE(summary.hasEventType());
        REQUIRE_FALSE(summary.hasLatitude());
        REQUIRE_FALSE(summary.hasLongitude());
        REQUIRE_FALSE(summary.hasDepth());
        REQUIRE_FALSE(summary.hasTime());
        REQUIRE_FALSE(summary.hasGeographicType());
        REQUIRE_FALSE(summary.hasReviewStatus());
        REQUIRE_THROWS_AS(summary.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(summary.getEventType(), std::runtime_error);
        REQUIRE_THROWS_AS(summary.getLatitude(), std::runtime_error);
        REQUIRE_THROWS_AS(summary.getLongitude(), std::runtime_error);
        REQUIRE_THROWS_AS(summary.getDepth(), std::runtime_error);
        REQUIRE_THROWS_AS(summary.getTime(), std::runtime_error);
        REQUIRE_THROWS_AS(summary.getGeographicType(), std::runtime_error);
        REQUIRE_THROWS_AS(summary.getReviewStatus(), std::runtime_error);

        // The columns the outer joins can leave null come back empty
        // rather than as a zero somebody would plot or believe.
        REQUIRE(summary.getCredit() == std::nullopt);
        REQUIRE(summary.getOriginSource() == std::nullopt);
        REQUIRE(summary.getMaximumAzimuthalGap() == std::nullopt);
        REQUIRE(summary.getWeightedRootMeanSquaredError() == std::nullopt);
        REQUIRE(summary.getNumberOfDefiningPhases() == std::nullopt);
        REQUIRE(summary.getMagnitudeValue() == std::nullopt);
        REQUIRE(summary.getMagnitudeType() == std::nullopt);

        // Version is the one field with a meaningful default: an event
        // nobody has revised is version zero, not "unset".
        REQUIRE(summary.getVersion() == 0);
    }
    SECTION("Set and get")
    {
        EventSummary summary;
        summary.setIdentifier(80001234);
        summary.setEventType(Event::EventType::Earthquake);
        summary.setVersion(3);
        summary.setLatitude(40.77);
        summary.setLongitude(-111.89);
        summary.setDepth(5000);
        summary.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
        summary.setCredit("tflynn");
        summary.setOriginSource("Jiggle");
        summary.setMaximumAzimuthalGap(112.5);
        summary.setWeightedRootMeanSquaredError(0.14);
        summary.setNumberOfDefiningPhases(22);
        summary.setGeographicType(Origin::GeographicType::Local);
        summary.setReviewStatus(Origin::ReviewStatus::Human);
        summary.setMagnitudeValue(3.4);
        summary.setMagnitudeType(IMagnitude::Type::Local);

        REQUIRE(summary.getIdentifier() == 80001234);
        REQUIRE(summary.getEventType() == Event::EventType::Earthquake);
        REQUIRE(summary.getVersion() == 3);
        REQUIRE(summary.getLatitude() == Catch::Approx(40.77));
        REQUIRE(summary.getDepth() == Catch::Approx(5000));
        REQUIRE(summary.getTime()
                == std::chrono::nanoseconds{1'700'000'000'000'000'000});
        REQUIRE(summary.getGeographicType() == Origin::GeographicType::Local);
        REQUIRE(summary.getReviewStatus() == Origin::ReviewStatus::Human);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*summary.getCredit() == "tflynn");
        REQUIRE(*summary.getOriginSource() == "Jiggle");
        REQUIRE(*summary.getMaximumAzimuthalGap() == Catch::Approx(112.5));
        REQUIRE(*summary.getWeightedRootMeanSquaredError()
                == Catch::Approx(0.14));
        REQUIRE(*summary.getNumberOfDefiningPhases() == 22);
        REQUIRE(*summary.getMagnitudeValue() == Catch::Approx(3.4));
        REQUIRE(*summary.getMagnitudeType() == IMagnitude::Type::Local);
        //NOLINTEND(bugprone-unchecked-optional-access)
    }
    SECTION("Longitude is normalized to [0, 360)")
    {
        // The same convention Origin and Station use, so a summary and the
        // event it summarizes cannot disagree about where a point is.
        EventSummary summary;
        summary.setLongitude(-111.89);
        REQUIRE(summary.getLongitude() == Catch::Approx(248.11));
        summary.setLongitude(360.0);
        REQUIRE(summary.getLongitude() == Catch::Approx(0.0));
    }
    SECTION("Bounds match the classes this flattens")
    {
        EventSummary summary;
        REQUIRE_THROWS_AS(summary.setLatitude(-90.0001), std::invalid_argument);
        REQUIRE_THROWS_AS(summary.setLatitude(90.0001), std::invalid_argument);
        REQUIRE_NOTHROW(summary.setLatitude(-90.0));
        REQUIRE_NOTHROW(summary.setLatitude(90.0));

        REQUIRE_THROWS_AS(summary.setDepth(-10000.1), std::invalid_argument);
        REQUIRE_THROWS_AS(summary.setDepth(1000000.1), std::invalid_argument);
        REQUIRE_NOTHROW(summary.setDepth(-10000.0));
        REQUIRE_NOTHROW(summary.setDepth(1000000.0));

        // Deliberately the same ceiling IMagnitude enforces - a summary
        // that rejected a magnitude the magnitude class accepts would make
        // the catalog and the event detail disagree.
        REQUIRE_NOTHROW(summary.setMagnitudeValue(11.0));
        REQUIRE_THROWS_AS(summary.setMagnitudeValue(11.0001),
                          std::invalid_argument);
        REQUIRE_NOTHROW(summary.setMagnitudeValue(-1.5));
    }
    SECTION("A one-station origin has a gap of the whole circle")
    {
        // 360 is a real value, not an off-by-one: with a single station
        // there is no second azimuth to close the gap with.
        EventSummary summary;
        REQUIRE_NOTHROW(summary.setMaximumAzimuthalGap(0.0));
        REQUIRE_NOTHROW(summary.setMaximumAzimuthalGap(359.9));
        REQUIRE_NOTHROW(summary.setMaximumAzimuthalGap(360.0));
        REQUIRE(summary.getMaximumAzimuthalGap().has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*summary.getMaximumAzimuthalGap() == Catch::Approx(360.0));

        REQUIRE_THROWS_AS(summary.setMaximumAzimuthalGap(360.0001),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(summary.setMaximumAzimuthalGap(-0.1),
                          std::invalid_argument);
    }
    SECTION("Nonsense location quality is rejected")
    {
        EventSummary summary;
        // A negative misfit is not a very good fit.
        REQUIRE_THROWS_AS(summary.setWeightedRootMeanSquaredError(-0.1),
                          std::invalid_argument);
        REQUIRE_NOTHROW(summary.setWeightedRootMeanSquaredError(0.0));
        // An origin located by no phases at all was not located.
        REQUIRE_THROWS_AS(summary.setNumberOfDefiningPhases(0),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(summary.setNumberOfDefiningPhases(-1),
                          std::invalid_argument);
        REQUIRE_NOTHROW(summary.setNumberOfDefiningPhases(1));
    }
    SECTION("A version counts up from zero")
    {
        EventSummary summary;
        REQUIRE_THROWS_AS(summary.setVersion(-1), std::invalid_argument);
        REQUIRE_NOTHROW(summary.setVersion(0));
        REQUIRE(summary.getVersion() == 0);
    }
    SECTION("A blank origin source is no origin source")
    {
        EventSummary summary;
        summary.setOriginSource("  Binder  ");
        REQUIRE(summary.getOriginSource().has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*summary.getOriginSource() == "Binder");

        summary.setOriginSource("   ");
        REQUIRE(summary.getOriginSource() == std::nullopt);
        summary.setOriginSource("");
        REQUIRE(summary.getOriginSource() == std::nullopt);
    }
    SECTION("A blank credit is no credit")
    {
        // The column is nullable for an automatic location, and an empty
        // string would claim somebody computed this while naming nobody.
        EventSummary summary;
        summary.setCredit("  tflynn  ");
        REQUIRE(summary.getCredit().has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*summary.getCredit() == "tflynn");

        summary.setCredit("   ");
        REQUIRE(summary.getCredit() == std::nullopt);
        summary.setCredit("");
        REQUIRE(summary.getCredit() == std::nullopt);
    }
    SECTION("Copy is a deep, independent copy")
    {
        EventSummary summary;
        summary.setIdentifier(80001234);
        summary.setVersion(2);
        summary.setLatitude(40.77);
        summary.setCredit("tflynn");
        summary.setMagnitudeValue(3.4);

        const EventSummary copy{summary};
        summary.setVersion(3);
        summary.setLatitude(41.0);
        summary.setCredit("someone-else");

        REQUIRE(copy.getIdentifier() == 80001234);
        REQUIRE(copy.getVersion() == 2);
        REQUIRE(copy.getLatitude() == Catch::Approx(40.77));
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*copy.getCredit() == "tflynn");
        REQUIRE(*copy.getMagnitudeValue() == Catch::Approx(3.4));
        //NOLINTEND(bugprone-unchecked-optional-access)
        REQUIRE_FALSE(copy.hasDepth());
    }
    SECTION("Copy assignment carries every field")
    {
        // The copy goes through the whole impl rather than a member list,
        // so a field added later cannot be silently dropped.
        EventSummary summary;
        summary.setIdentifier(80001234);
        summary.setEventType(Event::EventType::QuarryBlast);
        summary.setVersion(7);
        summary.setLatitude(40.77);
        summary.setLongitude(-111.89);
        summary.setDepth(1000);
        summary.setTime(std::chrono::nanoseconds{42});
        summary.setCredit("tflynn");
        summary.setOriginSource("Jiggle");
        summary.setMaximumAzimuthalGap(90.0);
        summary.setWeightedRootMeanSquaredError(0.2);
        summary.setNumberOfDefiningPhases(11);
        summary.setGeographicType(Origin::GeographicType::Regional);
        summary.setReviewStatus(Origin::ReviewStatus::Finalized);
        summary.setMagnitudeValue(2.2);
        summary.setMagnitudeType(IMagnitude::Type::Duration);

        EventSummary assigned;
        assigned = summary;

        REQUIRE(assigned.getIdentifier() == 80001234);
        REQUIRE(assigned.getEventType() == Event::EventType::QuarryBlast);
        REQUIRE(assigned.getVersion() == 7);
        REQUIRE(assigned.getLatitude() == Catch::Approx(40.77));
        REQUIRE(assigned.getLongitude() == Catch::Approx(248.11));
        REQUIRE(assigned.getDepth() == Catch::Approx(1000));
        REQUIRE(assigned.getTime() == std::chrono::nanoseconds{42});
        REQUIRE(assigned.getGeographicType()
                == Origin::GeographicType::Regional);
        REQUIRE(assigned.getReviewStatus()
                == Origin::ReviewStatus::Finalized);
        //NOLINTBEGIN(bugprone-unchecked-optional-access)
        REQUIRE(*assigned.getCredit() == "tflynn");
        REQUIRE(*assigned.getOriginSource() == "Jiggle");
        REQUIRE(*assigned.getMaximumAzimuthalGap() == Catch::Approx(90.0));
        REQUIRE(*assigned.getWeightedRootMeanSquaredError()
                == Catch::Approx(0.2));
        REQUIRE(*assigned.getNumberOfDefiningPhases() == 11);
        REQUIRE(*assigned.getMagnitudeValue() == Catch::Approx(2.2));
        REQUIRE(*assigned.getMagnitudeType() == IMagnitude::Type::Duration);
        //NOLINTEND(bugprone-unchecked-optional-access)
    }
    SECTION("Move")
    {
        EventSummary toMove;
        toMove.setIdentifier(80001234);
        toMove.setVersion(4);
        toMove.setCredit("tflynn");
        const EventSummary moved{std::move(toMove)};
        REQUIRE(moved.getIdentifier() == 80001234);
        REQUIRE(moved.getVersion() == 4);
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*moved.getCredit() == "tflynn");
    }
}

namespace
{
/// @note Concepts rather than a bare requires-expression on the concrete
///       type: a requires-expression only evaluates to false for a
///       DEPENDENT expression.  Written against SubnetTrigger directly the
///       member lookup happens immediately and the compiler simply errors,
///       so the guard has to go through a template parameter to work at
///       all.
template<typename T>
concept HasPosition = requires (T type)
{
    type.getLatitude();
    type.getLongitude();
    type.getDepth();
};

template<typename T>
concept HasReview = requires (T type)
{
    type.getEventType();
    type.getReviewStatus();
};
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::SubnetTrigger",
          "SubnetTrigger")
{
    SECTION("Defaults")
    {
        const SubnetTrigger trigger;
        REQUIRE_FALSE(trigger.hasEventIdentifier());
        REQUIRE_FALSE(trigger.hasTime());
        REQUIRE_THROWS_AS(trigger.getEventIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(trigger.getTime(), std::runtime_error);
        REQUIRE(trigger.getOriginSource() == std::nullopt);
    }
    SECTION("Set and get")
    {
        SubnetTrigger trigger;
        trigger.setEventIdentifier(80001234);
        trigger.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
        trigger.setOriginSource("real time test computer");

        REQUIRE(trigger.getEventIdentifier() == 80001234);
        REQUIRE(trigger.getTime()
                == std::chrono::nanoseconds{1'700'000'000'000'000'000});
        REQUIRE(trigger.getOriginSource().has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*trigger.getOriginSource() == "real time test computer");
    }
    SECTION("A blank origin source names no machine")
    {
        SubnetTrigger trigger;
        trigger.setOriginSource("  rtdb1  ");
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*trigger.getOriginSource() == "rtdb1");

        trigger.setOriginSource("   ");
        REQUIRE(trigger.getOriginSource() == std::nullopt);
        trigger.setOriginSource("");
        REQUIRE(trigger.getOriginSource() == std::nullopt);
    }
    SECTION("It carries no position to be plotted")
    {
        // The point of the type.  A trigger's row does have a latitude and
        // longitude in AQMS - the columns are NOT NULL - but they are
        // placeholders, and 0, 0 is in the Atlantic.  There is deliberately
        // no way to ask this for one, and this fails to compile if somebody
        // helpfully adds one back.
        static_assert(!::HasPosition<SubnetTrigger>);
        // Nor an event type or review status: anything still a trigger is
        // unreviewed by construction, because reviewing one turns it into
        // an event.
        static_assert(!::HasReview<SubnetTrigger>);
        // And the guard itself works - EventSummary does have these.
        static_assert(::HasPosition<EventSummary>);
        static_assert(::HasReview<EventSummary>);
    }
    SECTION("Copy is a deep, independent copy")
    {
        SubnetTrigger trigger;
        trigger.setEventIdentifier(80001234);
        trigger.setTime(std::chrono::nanoseconds{42});
        trigger.setOriginSource("rtdb1");

        const SubnetTrigger copy{trigger};
        trigger.setEventIdentifier(80009999);
        trigger.setOriginSource("rtdb2");

        REQUIRE(copy.getEventIdentifier() == 80001234);
        REQUIRE(copy.getTime() == std::chrono::nanoseconds{42});
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*copy.getOriginSource() == "rtdb1");
    }
    SECTION("Copy assignment carries every field")
    {
        SubnetTrigger trigger;
        trigger.setEventIdentifier(80001234);
        trigger.setTime(std::chrono::nanoseconds{42});
        trigger.setOriginSource("rtdb1");

        SubnetTrigger assigned;
        assigned = trigger;
        REQUIRE(assigned.getEventIdentifier() == 80001234);
        REQUIRE(assigned.getTime() == std::chrono::nanoseconds{42});
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*assigned.getOriginSource() == "rtdb1");
    }
    SECTION("Move")
    {
        SubnetTrigger toMove;
        toMove.setEventIdentifier(80001234);
        toMove.setTime(std::chrono::nanoseconds{42});
        toMove.setOriginSource("rtdb1");
        const SubnetTrigger moved{std::move(toMove)};
        REQUIRE(moved.getEventIdentifier() == 80001234);
        REQUIRE(moved.getTime() == std::chrono::nanoseconds{42});
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*moved.getOriginSource() == "rtdb1");
    }
}


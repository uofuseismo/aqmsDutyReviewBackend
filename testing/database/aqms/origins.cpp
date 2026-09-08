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
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Origin", "Origin")
{
    SECTION("Defaults")
    {
        const Origin origin;
        REQUIRE(origin.isPreferred());
        REQUIRE(origin.getArrivals().empty());
        REQUIRE_FALSE(origin.hasIdentifier());
        REQUIRE_FALSE(origin.hasLatitude());
        REQUIRE_FALSE(origin.hasLongitude());
        REQUIRE_FALSE(origin.hasDepth());
        REQUIRE_FALSE(origin.hasTime());
        REQUIRE_FALSE(origin.hasGeographicType());
        REQUIRE_FALSE(origin.hasReviewStatus());
        REQUIRE_THROWS_AS(origin.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(origin.getLatitude(), std::runtime_error);
        REQUIRE_THROWS_AS(origin.getLongitude(), std::runtime_error);
        REQUIRE_THROWS_AS(origin.getDepth(), std::runtime_error);
        REQUIRE_THROWS_AS(origin.getTime(), std::runtime_error);
        REQUIRE_THROWS_AS(origin.getGeographicType(), std::runtime_error);
        REQUIRE_THROWS_AS(origin.getReviewStatus(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        constexpr std::chrono::nanoseconds time{1'700'000'000'000'000'000};
        Origin origin;
        origin.setIdentifier(55);
        origin.setLatitude(40.77);
        origin.setDepth(5000);
        origin.setTime(time);
        origin.setGeographicType(Origin::GeographicType::Local);
        origin.setReviewStatus(Origin::ReviewStatus::Finalized);
        origin.setNotPreferred();

        REQUIRE(origin.getIdentifier() == 55);
        REQUIRE(origin.getLatitude() == 40.77);
        REQUIRE(origin.getDepth() == 5000);
        REQUIRE(origin.getTime() == time);
        REQUIRE(origin.getGeographicType() == Origin::GeographicType::Local);
        REQUIRE(origin.getReviewStatus() == Origin::ReviewStatus::Finalized);
        REQUIRE_FALSE(origin.isPreferred());
    }
    SECTION("Latitude bounds")
    {
        Origin origin;
        REQUIRE_NOTHROW(origin.setLatitude(-90.0));
        REQUIRE_NOTHROW(origin.setLatitude(90.0));
        REQUIRE_THROWS_AS(origin.setLatitude(-90.001), std::invalid_argument);
        REQUIRE_THROWS_AS(origin.setLatitude(90.001), std::invalid_argument);
    }
    SECTION("Depth bounds")
    {
        Origin origin;
        // AQMS's ORIGIN04 bounds, in meters: anything the database is
        // willing to store, this reads.
        REQUIRE_NOTHROW(origin.setDepth(-10000.0));
        REQUIRE_NOTHROW(origin.setDepth(1000000.0));
        REQUIRE_THROWS_AS(origin.setDepth(-10000.1), std::invalid_argument);
        REQUIRE_THROWS_AS(origin.setDepth(1000000.1), std::invalid_argument);
    }
    SECTION("Longitude is normalized to [0, 360)")
    {
        Origin origin;
        origin.setLongitude(-111.89);
        REQUIRE(origin.getLongitude() == Catch::Approx(248.11));

        origin.setLongitude(370.0);
        REQUIRE(origin.getLongitude() == Catch::Approx(10.0));

        origin.setLongitude(360.0);
        REQUIRE(origin.getLongitude() == Catch::Approx(0.0));

        origin.setLongitude(180.0);
        REQUIRE(origin.getLongitude() == Catch::Approx(180.0));
    }
    SECTION("Arrivals round trip")
    {
        Arrival arrival1;
        arrival1.setIdentifier(1);
        arrival1.setPhase(Arrival::Phase::P);
        arrival1.setStreamIdentifier(makeStreamIdentifier());
        arrival1.setTime(std::chrono::seconds {1784745696});
        arrival1.setReviewStatus(Arrival::ReviewStatus::Automatic);

        Arrival arrival2;
        arrival2.setIdentifier(2);
        arrival2.setPhase(Arrival::Phase::S);
        arrival2.setStreamIdentifier(makeStreamIdentifier());
        arrival2.setTime(std::chrono::seconds {1784745697});
        arrival2.setReviewStatus(Arrival::ReviewStatus::Human);

        Origin origin;
        origin.setArrivals(std::vector<Arrival> {arrival1, arrival2});
        const auto arrivals = origin.getArrivals();
        REQUIRE(arrivals.size() == 2);
        REQUIRE(arrivals.at(0).getIdentifier() == 1);
        REQUIRE(arrivals.at(1).getStreamIdentifier().getStation() == "CTU");
        REQUIRE(arrivals.at(1).getIdentifier() == 2);
    }
    SECTION("A blank credit is no credit")
    {
        // origin.subsource is legitimately empty for an automatic
        // location, so an empty string would claim somebody computed this
        // origin while naming nobody.
        auto origin = makeValidOrigin();
        REQUIRE(origin.getCredit() == std::nullopt);

        origin.setCredit("  tflynn ");
        REQUIRE(origin.getCredit().has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*origin.getCredit() == "tflynn");

        origin.setCredit("   ");
        REQUIRE(origin.getCredit() == std::nullopt);
        origin.setCredit("");
        REQUIRE(origin.getCredit() == std::nullopt);
    }
    SECTION("Copy is a deep, independent copy")
    {
        auto origin = makeValidOrigin();
        origin.setIdentifier(9);
        origin.setNotPreferred();

        const Origin copy{origin};
        origin.setIdentifier(10);
        REQUIRE(origin.getIdentifier() == 10);
        REQUIRE(copy.getIdentifier() == 9);
        REQUIRE(copy.getLatitude() == 40.77);
        REQUIRE_FALSE(copy.isPreferred());
    }
    SECTION("Move")
    {
        auto toMove = makeValidOrigin();
        toMove.setIdentifier(9);
        const Origin moved{std::move(toMove)};
        REQUIRE(moved.getIdentifier() == 9);
        REQUIRE(moved.hasDepth());
    }
    // TODO need iterator test 
}


/// Magnitudes belong to the ORIGIN - netmag.orid ties them - so a
/// relocation carries its own, and the same event can hold two values of
/// the same type that belong to different solutions.
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Origin magnitudes",
          "Origin")
{
    SECTION("Absent by default")
    {
        const Origin origin;
        REQUIRE_FALSE(origin.hasMagnitudes());
        REQUIRE_THROWS_AS(origin.getMagnitudes(), std::runtime_error);
        REQUIRE_THROWS_AS(origin.getPreferredMagnitude(), std::runtime_error);
    }
    SECTION("Magnitudes round trip and preserve derived types")
    {
        Origin origin;
        origin.setMagnitudes(makeMagnitudes());
        REQUIRE(origin.hasMagnitudes());

        const auto magnitudes = origin.getMagnitudes();
        REQUIRE(magnitudes.size() == 2);
        // clone() must have preserved the concrete subclass of each element.
        REQUIRE(magnitudes.at(0)->getType() == IMagnitude::Type::Local);
        REQUIRE(magnitudes.at(0)->getValue() == 3.4);
        REQUIRE(magnitudes.at(0)->isPreferred());
        REQUIRE(magnitudes.at(1)->getType() == IMagnitude::Type::Duration);
        REQUIRE(magnitudes.at(1)->getValue() == 3.1);
        REQUIRE_FALSE(magnitudes.at(1)->isPreferred());

        const auto preferred = origin.getPreferredMagnitude();
        REQUIRE(preferred->getType() == IMagnitude::Type::Local);
        REQUIRE(preferred->getValue() == 3.4);
    }
    SECTION("Empty magnitudes throw")
    {
        Origin origin;
        REQUIRE_THROWS_AS(
            origin.setMagnitudes(std::vector<std::unique_ptr<IMagnitude>> {}),
            std::invalid_argument);
    }
    SECTION("Null magnitude throws")
    {
        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.push_back(std::make_unique<LocalMagnitude> ());
        magnitudes.push_back(nullptr);
        Origin origin;
        REQUIRE_THROWS_AS(origin.setMagnitudes(magnitudes),
                          std::invalid_argument);
    }
    SECTION("Duplicate magnitude types throw")
    {
        auto first = std::make_unique<LocalMagnitude> ();
        first->setIsPreferred();
        auto second = std::make_unique<LocalMagnitude> ();
        second->setNotPreferred();
        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.push_back(std::move(first));
        magnitudes.push_back(std::move(second));
        Origin origin;
        REQUIRE_THROWS_AS(origin.setMagnitudes(magnitudes),
                          std::invalid_argument);
    }
    SECTION("Exactly one preferred magnitude is required")
    {
        Origin origin;

        std::vector<std::unique_ptr<IMagnitude>> nonePreferred;
        auto local = std::make_unique<LocalMagnitude> ();
        local->setNotPreferred();
        nonePreferred.push_back(std::move(local));
        REQUIRE_THROWS_AS(origin.setMagnitudes(nonePreferred),
                          std::invalid_argument);

        std::vector<std::unique_ptr<IMagnitude>> twoPreferred;
        auto localPref = std::make_unique<LocalMagnitude> ();
        localPref->setIsPreferred();
        auto durationPref = std::make_unique<DurationMagnitude> ();
        durationPref->setIsPreferred();
        twoPreferred.push_back(std::move(localPref));
        twoPreferred.push_back(std::move(durationPref));
        REQUIRE_THROWS_AS(origin.setMagnitudes(twoPreferred),
                          std::invalid_argument);
    }
    SECTION("They survive the origin being copied")
    {
        // Origin holds them by unique_ptr, so its copy has to clone each
        // one rather than let the compiler try to copy a move-only vector.
        Origin origin;
        origin.setIdentifier(9);
        origin.setMagnitudes(makeMagnitudes());

        const Origin copy{origin};
        REQUIRE(copy.hasMagnitudes());
        REQUIRE(copy.getMagnitudes().size() == 2);
        REQUIRE(copy.preferredMagnitude().getType()
                == IMagnitude::Type::Local);
        // A clone, not an alias into the original.
        REQUIRE(&copy.preferredMagnitude() != &origin.preferredMagnitude());
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Origin iterators", "Origin")
{
    SECTION("An origin with no arrivals iterates over nothing")
    {
        const Origin origin;
        REQUIRE(origin.size() == 0);
        REQUIRE(origin.begin() == origin.end());
        REQUIRE(origin.cbegin() == origin.cend());
        REQUIRE(std::distance(origin.begin(), origin.end()) == 0);

        int visited{0};
        for ([[maybe_unused]] const auto &arrival : origin){++visited;}
        REQUIRE(visited == 0);
    }
    SECTION("Iteration visits every arrival in time order")
    {
        // The property worth pinning: setArrivals sorts by time, so what
        // comes back is NOT what went in.  A caller reading arrivals in
        // iteration order is reading them chronologically whether or not
        // they built the vector that way.
        auto origin = makeOriginWithArrivals();
        REQUIRE(origin.size() == 3);
        REQUIRE(std::distance(origin.begin(), origin.end()) == 3);

        std::vector<int64_t> identifiers;
        for (const auto &arrival : origin)
        {
            identifiers.push_back(arrival.getIdentifier());
        }
        // Handed over as 3, 1, 2 - at times 300, 100, 200.
        REQUIRE(identifiers == std::vector<int64_t> {1, 2, 3});

        REQUIRE(std::is_sorted(origin.begin(), origin.end(),
                               [](const Arrival &lhs, const Arrival &rhs)
                               {
                                   return lhs.getTime() < rhs.getTime();
                               }));
    }
    SECTION("size, distance, and getArrivals agree")
    {
        const auto origin = makeOriginWithArrivals();
        const auto arrivals = origin.getArrivals();
        REQUIRE(arrivals.size() == origin.size());
        REQUIRE(static_cast<std::size_t>
                (std::distance(origin.begin(), origin.end()))
                == origin.size());
        for (std::size_t i = 0; i < origin.size(); ++i)
        {
            REQUIRE(arrivals.at(i).getIdentifier()
                    == origin.at(i).getIdentifier());
        }
    }
    SECTION("at, operator[] and the iterator name the same object")
    {
        auto origin = makeOriginWithArrivals();
        for (std::size_t i = 0; i < origin.size(); ++i)
        {
            const auto offset = static_cast<std::ptrdiff_t> (i);
            REQUIRE(&origin.at(i) == &origin[i]);
            REQUIRE(&origin.at(i) == &*std::next(origin.begin(), offset));
        }
    }
    SECTION("at is bounds checked")
    {
        // operator[] deliberately is not - it forwards to the vector's,
        // so an out-of-range index there is undefined and untestable.
        // at() is the one to reach for when the index came from outside.
        auto origin = makeOriginWithArrivals();
        REQUIRE_THROWS_AS(origin.at(3), std::out_of_range);
        REQUIRE_THROWS_AS(std::as_const(origin).at(3), std::out_of_range);

        const Origin empty;
        REQUIRE_THROWS_AS(empty.at(0), std::out_of_range);
    }
    SECTION("A const origin yields const iterators")
    {
        const auto origin = makeOriginWithArrivals();
        static_assert(std::is_same_v<decltype(origin.begin()),
                                     Origin::const_iterator>);
        static_assert(std::is_same_v<decltype(origin.cbegin()),
                                     Origin::const_iterator>);
        static_assert(std::is_same_v<decltype(origin.end()),
                                     Origin::const_iterator>);
        REQUIRE(std::distance(origin.begin(), origin.end()) == 3);
        REQUIRE(std::distance(origin.cbegin(), origin.cend()) == 3);
        REQUIRE(origin.begin() == origin.cbegin());
        REQUIRE(origin.begin()->getIdentifier() == 1);
    }
    SECTION("Access is read-only even through a non-const origin")
    {
        // There is no mutable iterator: setArrivals sorts by time and
        // rejects duplicate stream-and-phase pairs, and a caller who
        // could edit an arrival in place would leave neither holding
        // with nothing re-run to notice.
        auto origin = makeOriginWithArrivals();
        static_assert(std::is_same_v<Origin::iterator,
                                     Origin::const_iterator>);
        static_assert(std::is_same_v<decltype(origin.begin()),
                                     Origin::const_iterator>);
        static_assert(std::is_same_v<decltype(*origin.begin()),
                                     const Arrival &>);
        static_assert(std::is_same_v<decltype(origin.at(0)),
                                     const Arrival &>);
        static_assert(std::is_same_v<decltype(origin[0]),
                                     const Arrival &>);
        REQUIRE(origin.begin()->getIdentifier() == 1);
    }
    SECTION("Editing goes through getArrivals and back to setArrivals")
    {
        // The supported way to change an arrival, and the reason losing
        // the mutable iterator costs nothing: the round trip re-validates
        // and re-sorts, so the ordering cannot be quietly broken.
        auto origin = makeOriginWithArrivals();
        auto arrivals = origin.getArrivals();
        arrivals.at(0).setQuality(0.75);
        arrivals.at(0).setTime(std::chrono::seconds {400});
        origin.setArrivals(arrivals);

        // Moved to the end, because setArrivals sorted it there.
        REQUIRE(origin.size() == 3);
        REQUIRE(std::next(origin.begin(), 2)->getIdentifier() == 1);
        REQUIRE(std::as_const(origin).at(2).getQuality().has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*origin.at(2).getQuality() == Catch::Approx(0.75));
        REQUIRE(std::is_sorted(origin.begin(), origin.end(),
                               [](const Arrival &lhs, const Arrival &rhs)
                               {
                                   return lhs.getTime() < rhs.getTime();
                               }));
    }
    SECTION("Standard algorithms work over the range")
    {
        // These are vector iterators, so the whole of <algorithm> applies;
        // this is what makes exposing begin/end worth more than a
        // getArrivals() that copies.
        auto origin = makeOriginWithArrivals();
        const auto found
            = std::find_if(origin.begin(), origin.end(),
                           [](const Arrival &arrival)
                           {
                               return arrival.getIdentifier() == 2;
                           });
        REQUIRE(found != origin.end());
        REQUIRE(std::distance(origin.begin(), found) == 1);

        REQUIRE(std::count_if(origin.begin(), origin.end(),
                              [](const Arrival &arrival)
                              {
                                  return arrival.getPhase()
                                      == Arrival::Phase::P;
                              }) == 3);

        const auto earliest
            = std::min_element(origin.begin(), origin.end(),
                               [](const Arrival &lhs, const Arrival &rhs)
                               {
                                   return lhs.getTime() < rhs.getTime();
                               });
        REQUIRE(earliest == origin.begin());
    }
    SECTION("A copy iterates over its own arrivals")
    {
        auto origin = makeOriginWithArrivals();
        Origin copy{origin};
        REQUIRE(copy.size() == origin.size());
        // Deep copy: the same index in each names a different object.
        REQUIRE(&*copy.begin() != &*origin.begin());

        // Replacing the copy's arrivals leaves the original untouched.
        copy.setArrivals(std::vector<Arrival> {
            makeArrivalAt(999, "HHZ", std::chrono::seconds {10})});
        REQUIRE(copy.size() == 1);
        REQUIRE(copy.at(0).getIdentifier() == 999);
        REQUIRE(origin.size() == 3);
        REQUIRE(origin.at(0).getIdentifier() == 1);
    }
    SECTION("Replacing the arrivals replaces what iteration sees")
    {
        auto origin = makeOriginWithArrivals();
        REQUIRE(origin.size() == 3);
        origin.setArrivals(std::vector<Arrival> {
            makeArrivalAt(7, "HHZ", std::chrono::seconds {50})});
        REQUIRE(origin.size() == 1);
        REQUIRE(std::distance(origin.begin(), origin.end()) == 1);
        REQUIRE(origin.begin()->getIdentifier() == 7);
        REQUIRE(std::next(origin.begin()) == origin.end());
    }
}




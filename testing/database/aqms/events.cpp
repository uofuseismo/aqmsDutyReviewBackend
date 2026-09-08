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
/// event.prefmag names a magnitude that lives on an origin, so the event
/// stores the identifier and goes and finds it.  The pairing - which
/// origin computed the preferred magnitude - is what the review screen
/// actually plots, and it is free once the magnitudes hang off origins.
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Event preferred magnitude",
          "Event")
{
    const auto makeEventWithMagnitudes
        = [](const int64_t preferredMagnitudeIdentifier)
          {
              // The magnitudes go on the NON-preferred origin on purpose:
              // AQMS keeps event.prefmag and origin.prefmag separately and
              // they need not agree, so the lookup must not quietly assume
              // the preferred origin.
              auto preferredOrigin = makeValidOrigin(40.77, -111.89, 5000, 1);
              preferredOrigin.setIsPreferred();
              auto otherOrigin = makeValidOrigin(41.0, -112.0, 6000, 2);
              otherOrigin.setNotPreferred();
              otherOrigin.setMagnitudes(makeMagnitudes());

              Event event;
              event.setIdentifier(77);
              event.setOrigins(std::vector<Origin> {preferredOrigin,
                                                    otherOrigin});
              event.setPreferredMagnitudeIdentifier(
                  preferredMagnitudeIdentifier);
              return event;
          };

    SECTION("It is found on whichever origin holds it")
    {
        const auto event = makeEventWithMagnitudes(11);
        REQUIRE(event.hasPreferredMagnitude());
        REQUIRE(event.preferredMagnitude().getIdentifier() == 11);
        REQUIRE(event.preferredMagnitude().getType()
                == IMagnitude::Type::Local);
        REQUIRE(event.preferredMagnitude().getValue() == 3.4);
    }
    SECTION("The origin holding it is reachable")
    {
        // The association the visualization needs, without the caller
        // writing the search.
        const auto event = makeEventWithMagnitudes(11);
        REQUIRE(event.preferredMagnitudeOrigin().getIdentifier() == 2);
        // And it really is NOT the preferred origin here.
        REQUIRE(event.preferredOrigin().getIdentifier() == 1);
        REQUIRE(&event.preferredMagnitudeOrigin()
                != &event.preferredOrigin());
    }
    SECTION("It can name a magnitude that is not its origin's preferred one")
    {
        // origin.prefmag says Local; event.prefmag can still say Duration.
        const auto event = makeEventWithMagnitudes(12);
        REQUIRE(event.preferredMagnitude().getType()
                == IMagnitude::Type::Duration);
        REQUIRE(event.preferredMagnitudeOrigin().preferredMagnitude()
                    .getType() == IMagnitude::Type::Local);
    }
    SECTION("An identifier no origin holds is reported, not guessed at")
    {
        const auto event = makeEventWithMagnitudes(9999);
        REQUIRE_FALSE(event.hasPreferredMagnitude());
        REQUIRE_THROWS_AS(event.preferredMagnitude(), std::runtime_error);
        REQUIRE_THROWS_AS(event.preferredMagnitudeOrigin(),
                          std::runtime_error);
    }
    SECTION("No identifier at all is a different failure")
    {
        auto origin = makeValidOrigin();
        origin.setMagnitudes(makeMagnitudes());
        Event event;
        event.setOrigins(std::vector<Origin> {origin});
        REQUIRE_FALSE(event.hasPreferredMagnitudeIdentifier());
        REQUIRE_FALSE(event.hasPreferredMagnitude());
        REQUIRE_THROWS_AS(event.preferredMagnitude(), std::runtime_error);
    }
    SECTION("Origins carrying no magnitudes are skipped, not fatal")
    {
        // Every origin without magnitudes is simply passed over - only an
        // exhausted search is an error.
        const auto event = makeEventWithMagnitudes(11);
        REQUIRE_FALSE(event.preferredOrigin().hasMagnitudes());
        REQUIRE(event.hasPreferredMagnitude());
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Event", "Event")
{
    SECTION("Defaults")
    {
        const Event event;
        REQUIRE_FALSE(event.hasIdentifier());
        REQUIRE_FALSE(event.hasOrigins());
        REQUIRE_FALSE(event.hasPreferredMagnitudeIdentifier());
        REQUIRE_FALSE(event.hasPreferredMagnitude());
        REQUIRE_FALSE(event.hasEventType());
        REQUIRE_THROWS_AS(event.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getEventType(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getOrigins(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getPreferredOrigin(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getPreferredMagnitudeIdentifier(),
                          std::runtime_error);
        REQUIRE_THROWS_AS(event.getPreferredMagnitude(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        Event event;
        event.setIdentifier(2024);
        event.setEventType(Event::EventType::Earthquake);
        event.setOrigins(std::vector<Origin> {makeValidOrigin()});

        REQUIRE(event.getIdentifier() == 2024);
        REQUIRE(event.getEventType() == Event::EventType::Earthquake);
        REQUIRE(event.hasOrigins());
        REQUIRE(event.getOrigins().size() == 1);
    }
    SECTION("A version counts up from zero")
    {
        Event event;
        // An event nobody has revised is version zero, not "unset" - which
        // is why there is no hasVersion to ask.
        REQUIRE(event.getVersion() == 0);
        event.setVersion(5);
        REQUIRE(event.getVersion() == 5);
        REQUIRE_THROWS_AS(event.setVersion(-1), std::invalid_argument);
        REQUIRE(event.getVersion() == 5);
    }
    SECTION("The version survives a copy")
    {
        // EventImpl's copy assignment names every member by hand, so a
        // field added and not listed there is dropped by every copy.  This
        // is the guard against that.
        Event event;
        event.setIdentifier(1);
        event.setVersion(9);
        const Event copy{event};
        REQUIRE(copy.getVersion() == 9);

        Event assigned;
        assigned = event;
        REQUIRE(assigned.getVersion() == 9);
    }
    SECTION("Empty origins throw")
    {
        Event event;
        REQUIRE_THROWS_AS(event.setOrigins(std::vector<Origin> {}),
                          std::invalid_argument);
    }
    SECTION("Origins missing required fields throw")
    {
        Origin noLatitude;
        noLatitude.setLongitude(-111.89);
        noLatitude.setDepth(5000);
        noLatitude.setTime(std::chrono::nanoseconds{1});

        Origin noTime;
        noTime.setLatitude(40.77);
        noTime.setLongitude(-111.89);
        noTime.setDepth(5000);

        Event event;
        REQUIRE_THROWS_AS(event.setOrigins(std::vector<Origin> {noLatitude}),
                          std::invalid_argument);
        REQUIRE_THROWS_AS(event.setOrigins(std::vector<Origin> {noTime}),
                          std::invalid_argument);
    }
    SECTION("An origin without a depth is accepted")
    {
        // origin.depth is the one nullable locating column in AQMS, so a
        // solution whose free depth never converged has none.  Refusing it
        // would throw away every other origin on the event too.
        Origin bare;
        bare.setIdentifier(4242);
        bare.setLatitude(40.77);
        bare.setLongitude(-111.89);
        bare.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
        bare.setIsPreferred();
        REQUIRE_FALSE(bare.hasDepth());

        Event event;
        REQUIRE_NOTHROW(event.setOrigins(std::vector<Origin> {bare}));
        REQUIRE(event.hasOrigins());
        REQUIRE_FALSE(event.getPreferredOrigin().hasDepth());
    }
    SECTION("Exactly one preferred origin is required")
    {
        Event event;

        auto nonePreferred = makeValidOrigin();
        nonePreferred.setNotPreferred();
        REQUIRE_THROWS_AS(event.setOrigins(std::vector<Origin> {nonePreferred}),
                          std::invalid_argument);

        auto first = makeValidOrigin(10.0, 20.0, 1000);
        first.setIsPreferred();
        auto second = makeValidOrigin(40.77, -111.89, 5000);
        second.setIsPreferred();
        REQUIRE_THROWS_AS(event.setOrigins(std::vector<Origin> {first, second}),
                          std::invalid_argument);
    }
    SECTION("Preferred origin is selected")
    {
        auto notPreferred = makeValidOrigin(10.0, 20.0, 1000);
        notPreferred.setIdentifier(1);
        notPreferred.setNotPreferred();

        auto preferred = makeValidOrigin(40.77, -111.89, 5000);
        preferred.setIdentifier(2);
        preferred.setIsPreferred();

        Event event;
        event.setOrigins(std::vector<Origin> {notPreferred, preferred});
        REQUIRE(event.getOrigins().size() == 2);
        REQUIRE(event.getPreferredOrigin().getIdentifier() == 2);
    }
    SECTION("Copy is a deep, independent copy")
    {
        auto origin = makeValidOrigin();
        origin.setMagnitudes(makeMagnitudes());

        Event event;
        event.setIdentifier(2024);
        event.setEventType(Event::EventType::QuarryBlast);
        event.setOrigins(std::vector<Origin> {origin});
        event.setPreferredMagnitudeIdentifier(11);

        const Event copy{event};
        event.setIdentifier(2025);
        REQUIRE(event.getIdentifier() == 2025);
        REQUIRE(copy.getIdentifier() == 2024);
        REQUIRE(copy.getEventType() == Event::EventType::QuarryBlast);
        REQUIRE(copy.getOrigins().size() == 1);
        // The polymorphic magnitudes ride along on the origin and must
        // survive the deep copy intact.
        REQUIRE(copy.preferredOrigin().getMagnitudes().size() == 2);
        REQUIRE(copy.getPreferredMagnitude()->getType()
                == IMagnitude::Type::Local);
    }
    SECTION("Move")
    {
        auto origin = makeValidOrigin();
        origin.setMagnitudes(makeMagnitudes());

        Event toMove;
        toMove.setIdentifier(2024);
        toMove.setOrigins(std::vector<Origin> {origin});
        toMove.setPreferredMagnitudeIdentifier(11);
        const Event moved{std::move(toMove)};
        REQUIRE(moved.getIdentifier() == 2024);
        REQUIRE(moved.getPreferredOrigin().hasLatitude());
        REQUIRE(moved.preferredOrigin().getMagnitudes().size() == 2);
        REQUIRE(moved.getPreferredMagnitude()->getType()
                == IMagnitude::Type::Local);
    }
}


TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Event zero-copy views",
          "Event")
{
    const auto makeEvent = []()
    {
        auto origin1 = makeValidOrigin(40.77, -111.89, 5000, 100);
        origin1.setIsPreferred();
        auto origin2 = makeValidOrigin(41.00, -112.00, 6000, 200);
        origin2.setNotPreferred();

        origin1.setMagnitudes(makeMagnitudes());

        Event event;
        event.setIdentifier(77);
        event.setOrigins(std::vector<Origin> {origin1, origin2});
        event.setPreferredMagnitudeIdentifier(11);
        return event;
    };

    SECTION("The views alias the event's own storage")
    {
        // The property that makes them worth having: no copy, no clone.
        // Compared by address, because equal values would also pass if
        // these quietly copied.
        const auto event = makeEvent();
        const auto origins = event.origins();
        REQUIRE(origins.size() == 2);
        REQUIRE(&origins[0] == &event.preferredOrigin());

        const auto again = event.origins();
        REQUIRE(again.data() == origins.data());

        // The magnitudes are the ORIGIN's, and the event's preferred one
        // is found among them rather than held twice.
        const auto magnitudes = event.preferredOrigin().magnitudes();
        REQUIRE(magnitudes.size() == 2);
        REQUIRE(event.preferredOrigin().magnitudes().data()
                == magnitudes.data());
        REQUIRE(magnitudes[0].get() == &event.preferredMagnitude());
        REQUIRE(&event.preferredMagnitudeOrigin() == &event.preferredOrigin());
    }
    SECTION("The copying getters really do copy")
    {
        // The contrast that justifies the views existing at all.
        const auto event = makeEvent();
        const auto copied = event.getOrigins();
        REQUIRE(copied.size() == event.origins().size());
        REQUIRE(copied.data() != event.origins().data());

        const auto clonedMagnitudes
            = event.preferredOrigin().getMagnitudes();
        REQUIRE(clonedMagnitudes.size()
                == event.preferredOrigin().magnitudes().size());
        REQUIRE(clonedMagnitudes[0].get()
                != event.preferredOrigin().magnitudes()[0].get());
    }
    SECTION("Views carry the same contents as the copying getters")
    {
        const auto event = makeEvent();
        const auto copied = event.getOrigins();
        const auto view = event.origins();
        REQUIRE(copied.size() == view.size());
        for (std::size_t i = 0; i < view.size(); ++i)
        {
            REQUIRE(copied.at(i).getIdentifier() == view[i].getIdentifier());
        }
        REQUIRE(event.getPreferredOrigin().getIdentifier()
                == event.preferredOrigin().getIdentifier());
        REQUIRE(event.getPreferredMagnitude()->getType()
                == event.preferredMagnitude().getType());
    }
    SECTION("A view walks the whole event without copying anything")
    {
        // What the JSON path will actually look like.
        const auto event = makeEvent();
        int nOrigins{0};
        int nArrivals{0};
        for (const auto &origin : event.origins())
        {
            ++nOrigins;
            for ([[maybe_unused]] const auto &arrival : origin)
            {
                ++nArrivals;
            }
        }
        REQUIRE(nOrigins == 2);
        REQUIRE(nArrivals == 0);

        std::vector<IMagnitude::Type> types;
        for (const auto &magnitude : event.preferredOrigin().magnitudes())
        {
            types.push_back(magnitude->getType());
        }
        REQUIRE(types.size() == 2);
        REQUIRE(types.at(0) == IMagnitude::Type::Local);
        REQUIRE(types.at(1) == IMagnitude::Type::Duration);
    }
    SECTION("Standard algorithms work over the views")
    {
        const auto event = makeEvent();
        REQUIRE(std::count_if(event.origins().begin(), event.origins().end(),
                              [](const Origin &origin)
                              {
                                  return origin.isPreferred();
                              }) == 1);
        const auto magnitudes = event.preferredOrigin().magnitudes();
        REQUIRE(std::count_if(magnitudes.begin(), magnitudes.end(),
                              [](const std::unique_ptr<IMagnitude> &magnitude)
                              {
                                  return magnitude->isPreferred();
                              }) == 1);
    }
    SECTION("An empty event has nothing to view")
    {
        const Event event;
        REQUIRE_THROWS_AS(event.origins(), std::runtime_error);
        REQUIRE_THROWS_AS(event.preferredOrigin(), std::runtime_error);
        REQUIRE_THROWS_AS(event.preferredMagnitude(), std::runtime_error);
        REQUIRE_THROWS_AS(event.preferredMagnitudeOrigin(),
                          std::runtime_error);
    }
    SECTION("A view of a copied event is that copy's own storage")
    {
        const auto event = makeEvent();
        const Event copy{event};
        REQUIRE(copy.origins().size() == event.origins().size());
        REQUIRE(copy.origins().data() != event.origins().data());
        REQUIRE(copy.preferredOrigin().magnitudes()[0].get()
                != event.preferredOrigin().magnitudes()[0].get());
    }
    SECTION("The views are read-only")
    {
        const auto event = makeEvent();
        static_assert(std::is_same_v<decltype(event.origins()),
                                     std::span<const Origin>>);
        static_assert(std::is_same_v<decltype(event.preferredOrigin()),
                                     const Origin &>);
        static_assert(std::is_same_v<decltype(event.preferredMagnitude()),
                                     const IMagnitude &>);
        static_assert(std::is_same_v<decltype(event.origins()[0]),
                                     const Origin &>);
    }
}


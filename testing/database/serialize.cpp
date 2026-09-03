#include <cstdint>
#include <algorithm>
#include <chrono>
#include <string>
#include <vector>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/hash.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

/// @brief An event summary with every optional column populated.
EventSummary makeFullEventSummary(const int64_t identifier = 80001234)
{
    EventSummary summary;
    summary.setIdentifier(identifier);
    summary.setEventType(Event::EventType::Earthquake);
    summary.setVersion(3);
    summary.setLatitude(40.77);
    summary.setLongitude(-111.89);
    summary.setDepth(5000);
    summary.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
    summary.setGeographicType(Origin::GeographicType::Local);
    summary.setReviewStatus(Origin::ReviewStatus::Human);
    summary.setCredit("UU");
    summary.setOriginSource("rtdb1");
    summary.setMaximumAzimuthalGap(87.5);
    summary.setWeightedRootMeanSquaredError(0.12);
    summary.setNumberOfDefiningPhases(24);
    summary.setMagnitudeValue(2.34);
    summary.setMagnitudeType(IMagnitude::Type::Local);
    return summary;
}

}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS", "[serialize][catalog]")
{
    SECTION("The catalog is an object wrapping the events")
    {
        // The frontend caches a catalog and asks whether it is stale, so
        // the body has to have somewhere to hang the hash next to the
        // events.  A bare array would not.
        const auto [catalog, hash] = toJSON(std::vector<EventSummary> {});
        REQUIRE(catalog.contains("events"));
        REQUIRE(catalog.at("events").is_array());
    }
    SECTION("An empty catalog serializes to an empty array, not null")
    {
        const auto [catalog, hash] = toJSON(std::vector<EventSummary> {});
        REQUIRE(catalog.at("events").as_array().empty());
    }
    SECTION("Every field reaches the frontend under the agreed name")
    {
        const std::vector<EventSummary> events{::makeFullEventSummary()};
        const auto [catalog, hash] = toJSON(events);
        const auto &array = catalog.at("events").as_array();
        REQUIRE(array.size() == 1);
        const auto &item = array.at(0).as_object();

        REQUIRE(item.at("eventIdentifier").as_int64() == 80001234);
        REQUIRE(item.at("eventType").as_string() == "earthquake");
        REQUIRE(item.at("version").as_int64() == 3);
        REQUIRE(item.at("latitude").as_double()
                == Catch::Approx(40.77));
        // Normalized to [0, 360) on the way into the model, so the
        // frontend sees 248.11 and not the -111.89 that was set.
        REQUIRE(item.at("longitude").as_double()
                == Catch::Approx(248.11));
        // Meters, as the model holds it - the frontend divides, not the
        // backend.
        REQUIRE(item.at("depth").as_double() == Catch::Approx(5000));
        REQUIRE(item.at("originTime").as_int64()
                == 1'700'000'000'000'000'000);
        REQUIRE(item.at("geographicType").as_string() == "local");
        REQUIRE(item.at("reviewStatus").as_string() == "human");
        REQUIRE(item.at("credit").as_string() == "UU");
        REQUIRE(item.at("originSource").as_string() == "rtdb1");
        REQUIRE(item.at("maximumAzimuthalGap").as_double()
                == Catch::Approx(87.5));
        REQUIRE(item.at("weightedRootMeanSquaredError").as_double()
                == Catch::Approx(0.12));
        REQUIRE(item.at("numberOfDefiningPhases").as_int64() == 24);
        REQUIRE(item.at("magnitude").as_double() == Catch::Approx(2.34));
        REQUIRE(item.at("magnitudeType").as_string() == "local");
    }
    SECTION("Nothing AQMS did not say gets a key")
    {
        // An outer join that matched no row must not become a null the
        // frontend has to test for - the key is simply absent.
        EventSummary summary;
        summary.setIdentifier(80005678);
        summary.setVersion(1);
        const auto [catalog, hash] = toJSON(std::vector<EventSummary> {summary});
        const auto &item = catalog.at("events").as_array().at(0).as_object();
        REQUIRE(item.at("eventIdentifier").as_int64() == 80005678);
        REQUIRE(!item.contains("latitude"));
        REQUIRE(!item.contains("longitude"));
        REQUIRE(!item.contains("depth"));
        REQUIRE(!item.contains("originTime"));
        REQUIRE(!item.contains("credit"));
        REQUIRE(!item.contains("originSource"));
        REQUIRE(!item.contains("maximumAzimuthalGap"));
        REQUIRE(!item.contains("weightedRootMeanSquaredError"));
        REQUIRE(!item.contains("numberOfDefiningPhases"));
        REQUIRE(!item.contains("magnitude"));
        REQUIRE(!item.contains("magnitudeType"));
    }
    SECTION("The events keep the order the database returned")
    {
        const std::vector<EventSummary> events{::makeFullEventSummary(3),
                                               ::makeFullEventSummary(1),
                                               ::makeFullEventSummary(2)};
        const auto [catalog, hash] = toJSON(events);
        const auto &array = catalog.at("events").as_array();
        REQUIRE(array.at(0).as_object().at("eventIdentifier").as_int64() == 3);
        REQUIRE(array.at(1).as_object().at("eventIdentifier").as_int64() == 1);
        REQUIRE(array.at(2).as_object().at("eventIdentifier").as_int64() == 2);
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS", "[serialize][catalogHash]")
{
    // The /catalog and /catalog-hash routes both answer a frontend that
    // caches a catalog and only re-downloads when the hash it holds stops
    // matching.  That is only sound if there is exactly one answer to
    // "what is this catalog's hash", which is why serialization hands one
    // back rather than leaving each caller to compute its own.
    SECTION("The hash returned alongside is the hash in the body")
    {
        const std::vector<EventSummary> events{::makeFullEventSummary(1),
                                               ::makeFullEventSummary(2)};
        const auto [catalog, hash] = toJSON(events);
        REQUIRE(catalog.contains("hash"));
        REQUIRE(catalog.at("hash").as_string() == hash);
    }
    SECTION("It is a BLAKE2b digest as lower-case hex")
    {
        const auto [catalog, hash] = toJSON(std::vector<EventSummary> {});
        REQUIRE(hash.size() == 64);
        REQUIRE(std::ranges::all_of(hash,
                                    [](const char c)
                                    {
                                        return (c >= '0' && c <= '9') ||
                                               (c >= 'a' && c <= 'f');
                                    }));
    }
    SECTION("The hash covers the events and not the hash itself")
    {
        // A hash cannot cover itself, so the key has to go in after the
        // digest is taken.  Strip it back off and the bytes that were
        // hashed have to reappear exactly.
        const std::vector<EventSummary> events{::makeFullEventSummary()};
        auto [catalog, hash] = toJSON(events);
        catalog.erase("hash");
        REQUIRE(AQMSDutyReviewBackend::hash(boost::json::serialize(catalog))
                == hash);
    }
    SECTION("The same catalog hashes the same way twice")
    {
        const std::vector<EventSummary> events{::makeFullEventSummary(1),
                                               ::makeFullEventSummary(2)};
        REQUIRE(toJSON(events).second == toJSON(events).second);
    }
    SECTION("A changed event changes the hash")
    {
        const std::vector<EventSummary> events{::makeFullEventSummary()};
        auto revised = ::makeFullEventSummary();
        revised.setMagnitudeValue(2.35);
        REQUIRE(toJSON(events).second
             != toJSON(std::vector<EventSummary> {revised}).second);
    }
    SECTION("A reordered catalog changes the hash")
    {
        // The hash covers the body verbatim, so the order the events
        // arrive in is part of what the client is comparing.  The catalog
        // query's ORDER BY origin.datetime DESC is what keeps that
        // predictable; this is here so a change that drops it shows up as
        // a failing test rather than as clients re-downloading forever.
        const std::vector<EventSummary> ascending{::makeFullEventSummary(1),
                                                  ::makeFullEventSummary(2)};
        const std::vector<EventSummary> descending{::makeFullEventSummary(2),
                                                   ::makeFullEventSummary(1)};
        REQUIRE(toJSON(ascending).second != toJSON(descending).second);
    }
    SECTION("Events sharing an origin time are still distinct events")
    {
        // Why the catalog query orders by evid as well as datetime: a tie
        // is possible, and nothing here collapses one.  Two events at the
        // same instant are two entries, and which order they arrive in
        // changes the hash - so the total order has to come from the
        // query, which this cannot reach.
        auto first = ::makeFullEventSummary(1);
        auto second = ::makeFullEventSummary(2);
        first.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
        second.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
        const auto [catalog, hash]
            = toJSON(std::vector<EventSummary> {first, second});
        REQUIRE(catalog.at("events").as_array().size() == 2);
        REQUIRE(toJSON(std::vector<EventSummary> {second, first}).second
             != hash);
    }
    SECTION("An empty catalog still has a hash")
    {
        const auto [catalog, hash] = toJSON(std::vector<EventSummary> {});
        REQUIRE(!hash.empty());
        REQUIRE(toJSON(std::vector<EventSummary> {}).second != std::string {});
    }
}

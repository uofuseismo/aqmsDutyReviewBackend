#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/serialize.hpp>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/arrival.hpp"
#include "aqmsDutyReviewBackend/database/aqms/localMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/durationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
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

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS", "[serialize][eventLocks]")
{
    SECTION("An empty list serializes to an array, not null")
    {
        const auto locks = toJSON(std::vector<EventLock> {});
        REQUIRE(locks.is_array());
        REQUIRE(locks.as_array().empty());
    }
    SECTION("A lock carries who holds it and since when")
    {
        EventLock lock;
        lock.setEventIdentifier(31151051);
        lock.setUser("bbaker");
        lock.setAcquisitionTime("2026-09-04T20:43:26Z");
        const auto locks = toJSON(std::vector<EventLock> {lock});
        const auto &item = locks.as_array().at(0).as_object();
        REQUIRE(item.at("eventIdentifier").as_int64() == 31151051);
        REQUIRE(item.at("user").as_string() == "bbaker");
        REQUIRE(item.at("acquiredAt").as_string() == "2026-09-04T20:43:26Z");
    }
    SECTION("A lock with no lddate has no acquiredAt key")
    {
        // Absent rather than null - the same convention the catalog uses
        // for anything AQMS had nothing to say about.
        EventLock lock;
        lock.setEventIdentifier(31151053);
        lock.setUser("jsmith");
        const auto locks = toJSON(std::vector<EventLock> {lock});
        const auto &item = locks.as_array().at(0).as_object();
        REQUIRE(!item.contains("acquiredAt"));
        // Still a lock, and still reported as held.
        REQUIRE(item.at("user").as_string() == "jsmith");
    }
    SECTION("acquiredAt is the shape a browser can parse")
    {
        // The format is produced in SQL, so this cannot check the
        // conversion - but it can hold the contract the query is written
        // against, which is the same RFC 3339 UTC shape list_users emits.
        // The 'T' and the 'Z' are both load-bearing: Javascript's Date
        // rejects "2026-09-04T20:43:26-06" and accepts this.
        EventLock lock;
        lock.setEventIdentifier(1);
        lock.setUser("bbaker");
        lock.setAcquisitionTime("2026-09-04T20:43:26Z");
        const auto acquired
            = toJSON(std::vector<EventLock> {lock})
                  .as_array().at(0).as_object().at("acquiredAt").as_string();
        const std::string text{acquired.c_str()};
        REQUIRE(text.size() == 20);
        REQUIRE(text.at(10) == 'T');
        REQUIRE(text.back() == 'Z');
        REQUIRE(text.find('+') == std::string::npos);
    }
    SECTION("An empty acquisition time is refused rather than stored")
    {
        EventLock lock;
        REQUIRE_THROWS_AS(lock.setAcquisitionTime(""), std::invalid_argument);
        REQUIRE(!lock.hasAcquisitionTime());
        REQUIRE_THROWS_AS(lock.getAcquisitionTime(), std::runtime_error);
    }
    SECTION("Acquisition time survives a copy")
    {
        EventLock lock;
        lock.setEventIdentifier(7);
        lock.setUser("bbaker");
        lock.setAcquisitionTime("2026-09-04T20:43:26Z");
        const EventLock copy{lock};
        REQUIRE(copy.hasAcquisitionTime());
        REQUIRE(copy.getAcquisitionTime() == "2026-09-04T20:43:26Z");
    }
}

namespace
{

/// @brief An event with two origins: a preferred one carrying a local and
///        a moment magnitude, and an earlier one with a pick.
[[nodiscard]] Event makeDetailedEvent()
{
    Arrival arrival;
    arrival.setIdentifier(1);
    arrival.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
    arrival.setPhase(Arrival::Phase::P);
    arrival.setReviewStatus(Arrival::ReviewStatus::Human);
    arrival.setQuality(0.8);
    arrival.setResidual(std::chrono::nanoseconds{50'000'000});
    arrival.setSourceReceiverDistance(12.5);
    arrival.setSourceReceiverAzimuth(145.0);
    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork("UU");
    streamIdentifier.setStation("CTU");
    streamIdentifier.setChannel("HHZ");
    streamIdentifier.setLocationCode("01");
    arrival.setStreamIdentifier(streamIdentifier);

    auto local = std::make_unique<LocalMagnitude> ();
    local->setIdentifier(51);
    local->setValue(3.5);
    local->setIsPreferred();
    std::vector<std::unique_ptr<IMagnitude>> preferredMagnitudes;
    preferredMagnitudes.push_back(std::move(local));

    Origin preferred;
    preferred.setIdentifier(200);
    preferred.setLatitude(40.8);
    preferred.setLongitude(-111.8);
    preferred.setDepth(6000);
    preferred.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
    preferred.setIsPreferred();
    preferred.setMagnitudes(std::move(preferredMagnitudes));

    Origin superseded;
    superseded.setIdentifier(100);
    superseded.setLatitude(40.7);
    superseded.setLongitude(-111.9);
    superseded.setDepth(5000);
    superseded.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
    superseded.setNotPreferred();
    superseded.setArrivals(std::vector<Arrival> {arrival});

    Event event;
    event.setIdentifier(31152306);
    event.setEventType(Event::EventType::Earthquake);
    event.setVersion(1);
    event.setOrigins(std::vector<Origin> {preferred, superseded});
    event.setPreferredMagnitudeIdentifier(51);
    return event;
}

}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS", "[serialize][event]")
{
    SECTION("Every origin says whether it is the preferred one")
    {
        // Redundant against preferredOriginIdentifier and here anyway: a
        // client rendering a list asks this per row, and may well render
        // the non-preferred origins differently or not at all.
        const auto json = toJSON(::makeDetailedEvent());
        const auto &origins = json.at("origins").as_array();
        REQUIRE(origins.size() == 2);
        REQUIRE(origins.at(0).as_object().at("isPreferred").as_bool());
        REQUIRE_FALSE(origins.at(1).as_object().at("isPreferred").as_bool());
        // Present on every origin, not only the preferred one - a missing
        // key would mean "false" by omission, which is the sort of thing a
        // client gets wrong once.
        for (const auto &origin : origins)
        {
            REQUIRE(origin.as_object().contains("isPreferred"));
        }
    }
    SECTION("The flag agrees with the event's identifier")
    {
        const auto json = toJSON(::makeDetailedEvent());
        const auto preferredIdentifier
            = json.at("preferredOriginIdentifier").as_int64();
        for (const auto &entry : json.at("origins").as_array())
        {
            const auto &origin = entry.as_object();
            REQUIRE(origin.at("isPreferred").as_bool()
                    == (origin.at("originIdentifier").as_int64()
                        == preferredIdentifier));
        }
    }
    SECTION("Magnitudes are named by identifier, not flagged")
    {
        // Deliberately no isPreferred on a magnitude: it could not say
        // whether it means origin.prefmag or event.prefmag, and those can
        // differ.
        const auto json = toJSON(::makeDetailedEvent());
        const auto &preferredOrigin = json.at("origins").as_array()
                                          .at(0).as_object();
        REQUIRE(preferredOrigin.at("preferredMagnitudeIdentifier")
                    .as_int64() == 51);
        REQUIRE(json.at("preferredMagnitudeIdentifier").as_int64() == 51);
        for (const auto &magnitude
             : preferredOrigin.at("magnitudes").as_array())
        {
            REQUIRE_FALSE(magnitude.as_object().contains("isPreferred"));
        }
    }
    SECTION("Origins nest their arrivals and magnitudes")
    {
        const auto json = toJSON(::makeDetailedEvent());
        const auto &origins = json.at("origins").as_array();
        // Both keys are always present, even when empty, so a client can
        // iterate without a special case.
        for (const auto &entry : origins)
        {
            REQUIRE(entry.as_object().at("magnitudes").is_array());
            REQUIRE(entry.as_object().at("arrivals").is_array());
        }
        const auto &superseded = origins.at(1).as_object();
        REQUIRE(superseded.at("magnitudes").as_array().empty());
        const auto &arrival
            = superseded.at("arrivals").as_array().at(0).as_object();
        REQUIRE(arrival.at("station").as_string() == "CTU");
        REQUIRE(arrival.at("phase").as_string() == "P");
        REQUIRE(arrival.at("sourceReceiverDistance").as_double()
                == Catch::Approx(12.5));
        REQUIRE(arrival.at("sourceReceiverAzimuth").as_double()
                == Catch::Approx(145.0));
        REQUIRE(arrival.at("residual").as_int64() == 50'000'000);
    }
    SECTION("Trimming keeps the preferred origin whole")
    {
        const auto json = toJSON(::makeDetailedEvent(),
                                 OriginDetail::PreferredOriginOnly);
        const auto &origins = json.at("origins").as_array();
        REQUIRE(origins.size() == 2);
        // Same origins, same order - only the arrivals go.
        const auto &preferred = origins.at(0).as_object();
        REQUIRE(preferred.at("isPreferred").as_bool());
        REQUIRE(preferred.contains("arrivals"));
    }
    SECTION("A trimmed origin loses its arrivals and nothing else")
    {
        const auto json = toJSON(::makeDetailedEvent(),
                                 OriginDetail::PreferredOriginOnly);
        const auto &superseded = json.at("origins").as_array()
                                     .at(1).as_object();
        REQUIRE_FALSE(superseded.at("isPreferred").as_bool());
        // Absent, NOT an empty array - an empty one would claim this
        // origin has no picks, and it has one.
        REQUIRE_FALSE(superseded.contains("arrivals"));
        // Everything needed to list it beside the preferred solution
        // survives.
        REQUIRE(superseded.at("originIdentifier").as_int64() == 100);
        REQUIRE(superseded.contains("latitude"));
        REQUIRE(superseded.contains("longitude"));
        REQUIRE(superseded.contains("depth"));
        REQUIRE(superseded.contains("originTime"));
        REQUIRE(superseded.contains("magnitudes"));
    }
    SECTION("A trimmed origin still says how many picks it has")
    {
        // The point of the count: a client can render "1 pick" and decide
        // whether to go and ask for them.
        const auto trimmed = toJSON(::makeDetailedEvent(),
                                    OriginDetail::PreferredOriginOnly);
        const auto &superseded = trimmed.at("origins").as_array()
                                        .at(1).as_object();
        REQUIRE(superseded.at("arrivalCount").as_int64() == 1);
        REQUIRE_FALSE(superseded.contains("arrivals"));

        // And the count agrees with the array whenever both are present.
        const auto full = toJSON(::makeDetailedEvent());
        for (const auto &entry : full.at("origins").as_array())
        {
            const auto &origin = entry.as_object();
            REQUIRE(origin.at("arrivalCount").as_int64()
                    == static_cast<std::int64_t>
                       (origin.at("arrivals").as_array().size()));
        }
    }
    SECTION("The default is every origin in full")
    {
        // Existing callers keep what they had.
        const auto explicitly = toJSON(::makeDetailedEvent(),
                                       OriginDetail::AllOrigins);
        const auto byDefault = toJSON(::makeDetailedEvent());
        REQUIRE(boost::json::serialize(explicitly)
                == boost::json::serialize(byDefault));
        for (const auto &entry : byDefault.at("origins").as_array())
        {
            REQUIRE(entry.as_object().contains("arrivals"));
        }
    }
    SECTION("Longitude is normalized, as it is everywhere else")
    {
        // Not a serializer decision - Origin::setLongitude folds into
        // [0,360) so a summary and the event it summarizes cannot disagree
        // about where the same point is.  Pinned here because it is the
        // one field that looks wrong at a glance.
        const auto json = toJSON(::makeDetailedEvent());
        REQUIRE(json.at("origins").as_array().at(0).as_object()
                    .at("longitude").as_double() == Catch::Approx(248.2));
    }
}

#include <chrono>
#include <string>
#include <thread>
#include <boost/json/object.hpp>
#include <catch2/catch_test_macros.hpp>
#include "aqmsDutyReviewBackend/database/aqms/catalogCache.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

/// @brief A catalog with one recognisable event in it.
[[nodiscard]] boost::json::object makeCatalog(const std::int64_t identifier,
                                              const std::string &hash)
{
    boost::json::array events;
    boost::json::object event;
    event["eventIdentifier"] = identifier;
    events.push_back(std::move(event));

    boost::json::object catalog;
    catalog["events"] = std::move(events);
    catalog["hash"] = hash;
    return catalog;
}

}

/// The cache sits on the request path for every catalog poll, and the
/// thing it is trusted to get right is when NOT to answer.
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::CatalogCache",
          "catalogCache")
{
    SECTION("An empty cache answers nothing")
    {
        const CatalogCache cache;
        REQUIRE_FALSE(cache.lookup("some-token").has_value());
    }
    SECTION("A matching token is served from the cache")
    {
        CatalogCache cache;
        cache.store("token-a", ::makeCatalog(31152986, "abc"), "abc");

        const auto entry = cache.lookup("token-a");
        REQUIRE(entry.has_value());
        REQUIRE(entry->hash == "abc");
        REQUIRE(entry->catalog.at("events").as_array().at(0).as_object()
                      .at("eventIdentifier").as_int64() == 31152986);
    }
    SECTION("A different token is a miss, however fresh the entry")
    {
        // The whole point: the token is what says the catalog moved, and a
        // cache that answered anyway would serve a catalog the database
        // has already replaced.
        CatalogCache cache;
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        REQUIRE_FALSE(cache.lookup("token-b").has_value());
        // ...and the old entry is still there for its own token.
        REQUIRE(cache.lookup("token-a").has_value());
    }
    SECTION("Storing again replaces what was held")
    {
        CatalogCache cache;
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        cache.store("token-b", ::makeCatalog(2, "def"), "def");

        REQUIRE_FALSE(cache.lookup("token-a").has_value());
        const auto entry = cache.lookup("token-b");
        REQUIRE(entry.has_value());
        REQUIRE(entry->hash == "def");
    }
    SECTION("An entry past its maximum age is a miss on a matching token")
    {
        // The backstop.  The token now carries the catalog's window
        // bucket, so a slide changes it - but a cache with no upper bound
        // on staleness is a thing nobody wants to discover they have.
        CatalogCache cache{std::chrono::seconds {0}};   // clamped, see below
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        REQUIRE(cache.lookup("token-a").has_value());
    }
    SECTION("A non-positive maximum age is refused, not obeyed")
    {
        // Zero would expire every entry the instant it was stored, turning
        // the cache into a slower way of doing no caching at all.  The
        // constructor keeps its default instead.
        CatalogCache cache{std::chrono::seconds {-5}};
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        REQUIRE(cache.lookup("token-a").has_value());
    }
    SECTION("A short maximum age really does expire an entry")
    {
        CatalogCache cache{std::chrono::seconds {1}};
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        REQUIRE(cache.lookup("token-a").has_value());
        std::this_thread::sleep_for(std::chrono::milliseconds {1100});
        REQUIRE_FALSE(cache.lookup("token-a").has_value());
    }
    SECTION("Clearing forgets everything")
    {
        CatalogCache cache;
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        REQUIRE(cache.lookup("token-a").has_value());
        cache.clear();
        REQUIRE_FALSE(cache.lookup("token-a").has_value());
    }
    SECTION("Hits and misses are counted")
    {
        CatalogCache cache;
        REQUIRE(cache.getHitsAndMisses() == std::pair<int64_t, int64_t> {0, 0});

        cache.lookup("token-a");                    // miss - nothing stored
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        cache.lookup("token-a");                    // hit
        cache.lookup("token-a");                    // hit
        cache.lookup("token-b");                    // miss - wrong token

        const auto [hits, misses] = cache.getHitsAndMisses();
        REQUIRE(hits == 2);
        REQUIRE(misses == 2);
    }
    SECTION("The entry is a copy, not a view of the cache's storage")
    {
        // A caller holding an entry must not see it change under them when
        // something else stores a newer catalog.
        CatalogCache cache;
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        const auto entry = cache.lookup("token-a");
        REQUIRE(entry.has_value());

        cache.store("token-b", ::makeCatalog(2, "def"), "def");
        REQUIRE(entry->hash == "abc");
        REQUIRE(entry->catalog.at("events").as_array().at(0).as_object()
                      .at("eventIdentifier").as_int64() == 1);
    }
}

/// Crow runs one thread today.  A cache that silently required that would
/// be an unpleasant thing to discover the day numberOfThreads changes.
TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::CatalogCache is thread safe",
          "catalogCache")
{
    SECTION("Concurrent lookups and stores neither crash nor tear")
    {
        CatalogCache cache;
        cache.store("token-a", ::makeCatalog(1, "abc"), "abc");

        std::atomic<bool> stop{false};
        std::atomic<int> torn{0};
        std::vector<std::thread> readers;
        for (int i = 0; i < 4; ++i)
        {
            readers.emplace_back(
                [&]()
                {
                    while (!stop)
                    {
                        const auto entry = cache.lookup("token-a");
                        if (!entry){continue;}
                        // The hash and the catalog must belong to each
                        // other - never one entry's hash beside another's
                        // events.
                        const auto identifier
                            = entry->catalog.at("events").as_array().at(0)
                                    .as_object().at("eventIdentifier")
                                    .as_int64();
                        if (entry->hash == "abc" && identifier != 1)
                        {
                            torn = torn + 1;
                        }
                    }
                });
        }
        for (int i = 0; i < 200; ++i)
        {
            cache.store("token-a", ::makeCatalog(1, "abc"), "abc");
        }
        stop = true;
        for (auto &reader : readers){reader.join();}
        REQUIRE(torn == 0);
    }
}

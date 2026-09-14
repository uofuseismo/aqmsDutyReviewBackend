#include <string>
#include <boost/json/serialize.hpp>
#include <catch2/catch_test_macros.hpp>
#include "routes/routeCatalog.hpp"

TEST_CASE("routeCatalog", "[routeCatalog]")
{
    // The catalog is a process-wide registry, so each section adds to
    // whatever ran before it.  Record the starting size rather than
    // assuming it is empty.
    const auto startingSize = ::routeCatalog().size();

    SECTION("A route carries its method, path, metric and permission")
    {
        ::describeRoute("GET", "/events/<int>", "event",
                        AQMSDutyReviewBackend::Auth::Requirement
                        {AQMSDutyReviewBackend::Auth::IAuthenticator
                             ::Permissions::ReadOnly, false});
        const auto json = ::routeCatalogToJSON("test", "9.9.9");
        REQUIRE(json.at("name").as_string() == "test");
        REQUIRE(json.at("version").as_string() == "9.9.9");
        REQUIRE(json.at("routeCount").as_int64()
                == static_cast<std::int64_t> (startingSize + 1));

        bool found{false};
        for (const auto &entry : json.at("routes").as_array())
        {
            const auto &route = entry.as_object();
            if (route.at("path").as_string() != "/events/<int>"){continue;}
            found = true;
            REQUIRE(route.at("method").as_string() == "GET");
            REQUIRE(route.at("metric").as_string() == "event");
            // The spelling users.permission uses, not a second vocabulary.
            REQUIRE(route.at("permissions").as_string() == "read_only");
            // Absent, not false - it only appears where it is true.
            REQUIRE_FALSE(route.contains("requiresPassword"));
            // Prose comes from the summary table, keyed by metric.
            REQUIRE(route.at("summary").as_string().size() > 0);
        }
        REQUIRE(found);
    }
    SECTION("A route demanding a password says so")
    {
        ::describeRoute("POST", "/account/password", "account-password",
                        AQMSDutyReviewBackend::Auth::Requirement
                        {AQMSDutyReviewBackend::Auth::IAuthenticator
                             ::Permissions::ReadOnly, true});
        const auto json = ::routeCatalogToJSON("test", "9.9.9");
        bool found{false};
        for (const auto &entry : json.at("routes").as_array())
        {
            const auto &route = entry.as_object();
            if (route.at("path").as_string() != "/account/password"){continue;}
            found = true;
            REQUIRE(route.at("requiresPassword").as_bool());
        }
        REQUIRE(found);
    }
    SECTION("A route that authorizes nothing reports none")
    {
        ::describeOpenRoute("GET", "/auth/login", "auth-login");
        const auto json = ::routeCatalogToJSON("test", "9.9.9");
        bool found{false};
        for (const auto &entry : json.at("routes").as_array())
        {
            const auto &route = entry.as_object();
            if (route.at("path").as_string() != "/auth/login"){continue;}
            found = true;
            REQUIRE(route.at("permissions").as_string() == "none");
        }
        REQUIRE(found);
    }
    SECTION("An unsummarized route simply has no summary")
    {
        // The summary table is prose keyed by metric.  A metric missing
        // from it must not invent one, and must not drop the route.
        ::describeOpenRoute("GET", "/not-a-real-route", "no-such-metric");
        const auto json = ::routeCatalogToJSON("test", "9.9.9");
        bool found{false};
        for (const auto &entry : json.at("routes").as_array())
        {
            const auto &route = entry.as_object();
            if (route.at("path").as_string() != "/not-a-real-route"){continue;}
            found = true;
            REQUIRE_FALSE(route.contains("summary"));
        }
        REQUIRE(found);
    }
}

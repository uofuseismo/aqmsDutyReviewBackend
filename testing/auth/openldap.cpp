#include <cstdint>
#include <optional>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"

TEST_CASE("AQMSDutyReviewBackend::Auth", "OpenLDAPOptions")
{
    using namespace AQMSDutyReviewBackend::Auth;
    SECTION("Defaults")
    {
        const OpenLDAPOptions options;
        REQUIRE_FALSE(options.hasHost());
        REQUIRE(options.getPort() == 636);   
        REQUIRE(options.getOrganizationalUnit() == "");
        REQUIRE(options.getDomainComponent() == "");
        REQUIRE(options.maintainConnection() == false);
        REQUIRE(options.getVersion() == OpenLDAPOptions::Version::Three);
        REQUIRE_THROWS(options.getAddress());
        REQUIRE(options.getSuffix() == "");
        REQUIRE(options.getTLSVerifyClient() == std::nullopt);
        REQUIRE(options.isSecured() == true);
    } 
    SECTION("Options")
    {
        const std::string host = "localhost";
        const std::string org = "ou=redteam";
        const std::string dc = "dc=edu,dc=utah";
        const std::string suffix = org + "," + dc;
        constexpr auto tlsVerify{OpenLDAPOptions::TLSVerifyClient::Try};
        constexpr auto version{OpenLDAPOptions::Version::One};
        constexpr uint16_t port{456};
        OpenLDAPOptions options;
        REQUIRE_NOTHROW(options.setHost(host));
        options.setPort(port);
        options.setOrganizationalUnit(org);
        REQUIRE(options.getSuffix() == org);
        options.setDomainComponent(dc);
        options.setVersion(version);
        options.setTLSVerifyClient(tlsVerify);
        options.enableMaintainConnection();
        options.disableSecured();

        const OpenLDAPOptions copy{options};
        REQUIRE(copy.getPort() == port);   
        REQUIRE(copy.getOrganizationalUnit() == org);
        REQUIRE(copy.getDomainComponent() == dc);
        REQUIRE(copy.maintainConnection() == true);
        REQUIRE(copy.getVersion() == version);
        REQUIRE(copy.getHost() == host);
        REQUIRE(copy.getSuffix() == suffix);
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*copy.getTLSVerifyClient() == tlsVerify);
        REQUIRE_FALSE(copy.isSecured());
    }
}

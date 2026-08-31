#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <span>
#include <type_traits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
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

using namespace AQMSDutyReviewBackend::Database::AQMS;

namespace
{

/// @brief Builds a fully-populated stream identifier for reuse in tests.
StreamIdentifier makeStreamIdentifier()
{
    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork("UU");
    streamIdentifier.setStation("CTU");
    streamIdentifier.setChannel("HHZ");
    streamIdentifier.setLocationCode("01");
    return streamIdentifier;
}

/// @brief Builds an origin with all fields required by Event::setOrigins.
//NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Origin makeValidOrigin(const double latitude = 40.77,
                       const double longitude = -111.89,
                       const double depth = 5000,
                       const int64_t identifier = 3233)
{
    Origin origin;
    origin.setIdentifier(identifier);
    origin.setLatitude(latitude);
    origin.setLongitude(longitude);
    origin.setDepth(depth);
    origin.setTime(std::chrono::nanoseconds{1'700'000'000'000'000'000});
    return origin;
}

/// @brief Builds a valid magnitude set: a preferred local magnitude and a
///        non-preferred duration magnitude.
std::vector<std::unique_ptr<IMagnitude>> makeMagnitudes()
{
    auto local = std::make_unique<LocalMagnitude> ();
    local->setValue(3.4);
    local->setIsPreferred();

    auto duration = std::make_unique<DurationMagnitude> ();
    duration->setValue(3.1);
    duration->setNotPreferred();

    std::vector<std::unique_ptr<IMagnitude>> magnitudes;
    magnitudes.push_back(std::move(local));
    magnitudes.push_back(std::move(duration));
    return magnitudes;
}

/// @brief Builds a stream identifier on the given channel of UU.CTU.
StreamIdentifier makeStream(const std::string &channel)
{
    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork("UU");
    streamIdentifier.setStation("CTU");
    streamIdentifier.setChannel(channel);
    streamIdentifier.setLocationCode("01");
    return streamIdentifier;
}

/// @brief Builds a fully-populated peak-to-peak amplitude (millimeters).
PeakToPeakAmplitude makeAmplitude(const std::string &channel,
                                  const double amplitudeMillimeters)
{
    PeakToPeakAmplitude amplitude;
    amplitude.setStreamIdentifier(makeStream(channel));
    const std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>
        peakTimes{std::chrono::nanoseconds{1}, std::chrono::nanoseconds{2}};
    amplitude.setPeakTimes(peakTimes);
    amplitude.setAmplitude(amplitudeMillimeters,
                           PeakToPeakAmplitude::Units::Millimeters);
    return amplitude;
}

}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::StreamIdentifier", "StreamIdentifier")
{
    SECTION("Defaults")
    {
        const StreamIdentifier streamIdentifier;
        REQUIRE_FALSE(streamIdentifier.hasNetwork());
        REQUIRE_FALSE(streamIdentifier.hasStation());
        REQUIRE_FALSE(streamIdentifier.hasChannel());
        REQUIRE_FALSE(streamIdentifier.hasLocationCode());
        REQUIRE_THROWS_AS(streamIdentifier.getNetwork(), std::runtime_error);
        REQUIRE_THROWS_AS(streamIdentifier.getStation(), std::runtime_error);
        REQUIRE_THROWS_AS(streamIdentifier.getChannel(), std::runtime_error);
        REQUIRE_THROWS_AS(streamIdentifier.getLocationCode(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        const auto streamIdentifier = makeStreamIdentifier();
        REQUIRE(streamIdentifier.hasNetwork());
        REQUIRE(streamIdentifier.hasStation());
        REQUIRE(streamIdentifier.hasChannel());
        REQUIRE(streamIdentifier.hasLocationCode());
        REQUIRE(streamIdentifier.getNetwork() == "UU");
        REQUIRE(streamIdentifier.getStation() == "CTU");
        REQUIRE(streamIdentifier.getChannel() == "HHZ");
        REQUIRE(streamIdentifier.getLocationCode() == "01");
    }
    SECTION("Empty codes throw")
    {
        StreamIdentifier streamIdentifier;
        REQUIRE_THROWS_AS(streamIdentifier.setNetwork(""), std::invalid_argument);
        REQUIRE_THROWS_AS(streamIdentifier.setStation(""), std::invalid_argument);
        REQUIRE_THROWS_AS(streamIdentifier.setChannel(""), std::invalid_argument);
    }
    SECTION("Empty location code is valid")
    {
        StreamIdentifier streamIdentifier;
        streamIdentifier.setLocationCode("");
        REQUIRE(streamIdentifier.hasLocationCode());
        REQUIRE(streamIdentifier.getLocationCode().empty());
    }
    SECTION("Copy is a deep, independent copy")
    {
        auto streamIdentifier = makeStreamIdentifier();
        const StreamIdentifier copy{streamIdentifier};
        // Mutating the source must not disturb the copy.
        streamIdentifier.setNetwork("WY");
        REQUIRE(streamIdentifier.getNetwork() == "WY");
        REQUIRE(copy.getNetwork() == "UU");
        REQUIRE(copy.getStation() == "CTU");
        REQUIRE(copy.getChannel() == "HHZ");
        REQUIRE(copy.getLocationCode() == "01");
    }
    SECTION("Move")
    {
        auto toMove = makeStreamIdentifier();
        const StreamIdentifier moved{std::move(toMove)};
        REQUIRE(moved.getNetwork() == "UU");
        REQUIRE(moved.getChannel() == "HHZ");
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Arrival", "Arrival")
{
    SECTION("Defaults")
    {
        const Arrival arrival;
        REQUIRE_FALSE(arrival.hasIdentifier());
        REQUIRE_FALSE(arrival.hasTime());
        REQUIRE_FALSE(arrival.hasPhase());
        REQUIRE_FALSE(arrival.hasReviewStatus());
        REQUIRE_FALSE(arrival.hasStreamIdentifier());
        REQUIRE_FALSE(arrival.hasResidual());
        REQUIRE(arrival.getQuality() == std::nullopt);
        REQUIRE_THROWS_AS(arrival.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getTime(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getPhase(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getReviewStatus(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getStreamIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(arrival.getResidual(), std::runtime_error);
    }
    SECTION("Set and get")
    {
        constexpr int64_t identifier{482};
        constexpr std::chrono::nanoseconds time{1'700'000'000'123'456'789};
        constexpr std::chrono::nanoseconds residual{-25'000'000};
        constexpr double quality{0.75};

        Arrival arrival;
        arrival.setIdentifier(identifier);
        arrival.setTime(time);
        arrival.setPhase(Arrival::Phase::S);
        arrival.setReviewStatus(Arrival::ReviewStatus::Human);
        arrival.setStreamIdentifier(makeStreamIdentifier());
        arrival.setQuality(quality);
        arrival.setResidual(residual);

        REQUIRE(arrival.getIdentifier() == identifier);
        REQUIRE(arrival.getTime() == time);
        REQUIRE(arrival.getPhase() == Arrival::Phase::S);
        REQUIRE(arrival.getReviewStatus() == Arrival::ReviewStatus::Human);
        REQUIRE(arrival.hasStreamIdentifier());
        REQUIRE(arrival.getStreamIdentifier().getStation() == "CTU");
        const auto quality2 = arrival.getQuality();
        REQUIRE(quality2.has_value());
        //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
        REQUIRE(*quality2 == quality);
        REQUIRE(arrival.getResidual() == residual);
    }
    SECTION("Negative quality throws")
    {
        Arrival arrival;
        REQUIRE_THROWS_AS(arrival.setQuality(-0.1), std::invalid_argument);
        REQUIRE(arrival.getQuality() == std::nullopt);
    }
    SECTION("Incomplete stream identifier throws")
    {
        StreamIdentifier missingChannel;
        missingChannel.setNetwork("UU");
        missingChannel.setStation("CTU");
        missingChannel.setLocationCode("01");
        Arrival arrival;
        REQUIRE_THROWS_AS(arrival.setStreamIdentifier(missingChannel),
                          std::invalid_argument);
        REQUIRE_FALSE(arrival.hasStreamIdentifier());
    }
    SECTION("Copy is a deep, independent copy")
    {
        Arrival arrival;
        arrival.setIdentifier(7);
        arrival.setPhase(Arrival::Phase::P);
        arrival.setStreamIdentifier(makeStreamIdentifier());

        const Arrival copy{arrival};
        arrival.setIdentifier(8);
        REQUIRE(arrival.getIdentifier() == 8);
        REQUIRE(copy.getIdentifier() == 7);
        REQUIRE(copy.getPhase() == Arrival::Phase::P);
        REQUIRE(copy.getStreamIdentifier().getNetwork() == "UU");
    }
    SECTION("Move")
    {
        Arrival toMove;
        toMove.setIdentifier(7);
        toMove.setStreamIdentifier(makeStreamIdentifier());
        const Arrival moved{std::move(toMove)};
        REQUIRE(moved.getIdentifier() == 7);
        REQUIRE(moved.getStreamIdentifier().getChannel() == "HHZ");
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Magnitude", "Magnitude")
{
    SECTION("Local magnitude defaults")
    {
        const LocalMagnitude magnitude;
        REQUIRE(magnitude.getType() == IMagnitude::Type::Local);
        REQUIRE(magnitude.isPreferred());
        REQUIRE_FALSE(magnitude.hasIdentifier());
        REQUIRE_FALSE(magnitude.hasValue());
        REQUIRE_FALSE(magnitude.hasReviewStatus());
        REQUIRE_THROWS_AS(magnitude.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getValue(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getReviewStatus(), std::runtime_error);
    }
    SECTION("Types")
    {
        REQUIRE(LocalMagnitude{}.getType() == IMagnitude::Type::Local);
        REQUIRE(DurationMagnitude{}.getType() == IMagnitude::Type::Duration);
        REQUIRE(HumanMagnitude{}.getType() == IMagnitude::Type::Human);
    }
    SECTION("Set and get through base interface")
    {
        constexpr int64_t identifier{93};
        constexpr double value{3.4};
        LocalMagnitude magnitude;
        magnitude.setIdentifier(identifier);
        magnitude.setValue(value);
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Human);
        magnitude.setNotPreferred();

        REQUIRE(magnitude.getIdentifier() == identifier);
        REQUIRE(magnitude.getValue() == value);
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
        REQUIRE_FALSE(magnitude.isPreferred());

        magnitude.setIsPreferred();
        REQUIRE(magnitude.isPreferred());
    }
    SECTION("Value bounds")
    {
        LocalMagnitude magnitude;
        REQUIRE_NOTHROW(magnitude.setValue(11.0));
        REQUIRE(magnitude.getValue() == 11.0);
        REQUIRE_NOTHROW(magnitude.setValue(-1.5));   // Small events are negative.
        REQUIRE(magnitude.getValue() == -1.5);
        REQUIRE_THROWS_AS(magnitude.setValue(11.0001), std::invalid_argument);
    }
    SECTION("Human magnitude defaults to human reviewed")
    {
        const HumanMagnitude magnitude;
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
        // Always readable - the default guarantees a status, so no caller
        // has to check before reading one.
        REQUIRE(magnitude.hasReviewStatus());
    }
    SECTION("A human magnitude reports what the database said")
    {
        // An automatic human magnitude is incoherent, and AQMS may still
        // hold one.  This is a view of AQMS, so it comes back as stored.
        HumanMagnitude magnitude;
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Automatic);
        REQUIRE(magnitude.getReviewStatus()
                == IMagnitude::ReviewStatus::Automatic);
    }
    SECTION("Clone preserves derived type through the base pointer")
    {
        LocalMagnitude local;
        local.setIdentifier(3);
        local.setValue(4.2);
        // Hold it as a base pointer, as Event does, then clone.
        const std::unique_ptr<IMagnitude> base
            = std::make_unique<LocalMagnitude> (local);
        const auto cloned = base->clone();
        REQUIRE(cloned != nullptr);
        REQUIRE(cloned.get() != base.get());          // Distinct object.
        REQUIRE(cloned->getType() == IMagnitude::Type::Local);
        REQUIRE(cloned->getIdentifier() == 3);
        REQUIRE(cloned->getValue() == 4.2);
    }
    SECTION("Copy preserves base state and type independently")
    {
        DurationMagnitude magnitude;
        magnitude.setIdentifier(11);
        magnitude.setValue(2.1);
        magnitude.setNotPreferred();

        const DurationMagnitude copy{magnitude};
        magnitude.setValue(3.3);
        REQUIRE(magnitude.getValue() == 3.3);
        REQUIRE(copy.getType() == IMagnitude::Type::Duration);
        REQUIRE(copy.getIdentifier() == 11);
        REQUIRE(copy.getValue() == 2.1);
        REQUIRE_FALSE(copy.isPreferred());
    }
    SECTION("Move preserves base state and type")
    {
        DurationMagnitude toMove;
        toMove.setIdentifier(11);
        toMove.setValue(2.1);
        const DurationMagnitude moved{std::move(toMove)};
        REQUIRE(moved.getType() == IMagnitude::Type::Duration);
        REQUIRE(moved.getIdentifier() == 11);
        REQUIRE(moved.getValue() == 2.1);
    }
}

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

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Event", "Event")
{
    SECTION("Defaults")
    {
        const Event event;
        REQUIRE_FALSE(event.hasIdentifier());
        REQUIRE_FALSE(event.hasOrigins());
        REQUIRE_FALSE(event.hasMagnitudes());
        REQUIRE_FALSE(event.hasEventType());
        REQUIRE_THROWS_AS(event.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getEventType(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getOrigins(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getPreferredOrigin(), std::runtime_error);
        REQUIRE_THROWS_AS(event.getMagnitudes(), std::runtime_error);
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
    SECTION("Magnitudes round trip and preserve derived types")
    {
        Event event;
        event.setMagnitudes(makeMagnitudes());
        REQUIRE(event.hasMagnitudes());

        const auto magnitudes = event.getMagnitudes();
        REQUIRE(magnitudes.size() == 2);
        // clone() must have preserved the concrete subclass of each element.
        REQUIRE(magnitudes.at(0)->getType() == IMagnitude::Type::Local);
        REQUIRE(magnitudes.at(0)->getValue() == 3.4);
        REQUIRE(magnitudes.at(0)->isPreferred());
        REQUIRE(magnitudes.at(1)->getType() == IMagnitude::Type::Duration);
        REQUIRE(magnitudes.at(1)->getValue() == 3.1);
        REQUIRE_FALSE(magnitudes.at(1)->isPreferred());

        const auto preferred = event.getPreferredMagnitude();
        REQUIRE(preferred->getType() == IMagnitude::Type::Local);
        REQUIRE(preferred->getValue() == 3.4);
    }
    SECTION("Empty magnitudes throw")
    {
        Event event;
        REQUIRE_THROWS_AS(
            event.setMagnitudes(std::vector<std::unique_ptr<IMagnitude>> {}),
            std::invalid_argument);
    }
    SECTION("Null magnitude throws")
    {
        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.push_back(std::make_unique<LocalMagnitude> ());
        magnitudes.push_back(nullptr);
        Event event;
        REQUIRE_THROWS_AS(event.setMagnitudes(magnitudes),
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
        Event event;
        REQUIRE_THROWS_AS(event.setMagnitudes(magnitudes),
                          std::invalid_argument);
    }
    SECTION("Exactly one preferred magnitude is required")
    {
        Event event;

        std::vector<std::unique_ptr<IMagnitude>> nonePreferred;
        auto local = std::make_unique<LocalMagnitude> ();
        local->setNotPreferred();
        nonePreferred.push_back(std::move(local));
        REQUIRE_THROWS_AS(event.setMagnitudes(nonePreferred),
                          std::invalid_argument);

        std::vector<std::unique_ptr<IMagnitude>> twoPreferred;
        auto localPref = std::make_unique<LocalMagnitude> ();
        localPref->setIsPreferred();
        auto durationPref = std::make_unique<DurationMagnitude> ();
        durationPref->setIsPreferred();
        twoPreferred.push_back(std::move(localPref));
        twoPreferred.push_back(std::move(durationPref));
        REQUIRE_THROWS_AS(event.setMagnitudes(twoPreferred),
                          std::invalid_argument);
    }
    SECTION("Copy is a deep, independent copy")
    {
        Event event;
        event.setIdentifier(2024);
        event.setEventType(Event::EventType::QuarryBlast);
        event.setOrigins(std::vector<Origin> {makeValidOrigin()});
        event.setMagnitudes(makeMagnitudes());

        const Event copy{event};
        event.setIdentifier(2025);
        REQUIRE(event.getIdentifier() == 2025);
        REQUIRE(copy.getIdentifier() == 2024);
        REQUIRE(copy.getEventType() == Event::EventType::QuarryBlast);
        REQUIRE(copy.getOrigins().size() == 1);
        // The polymorphic magnitudes must survive the deep copy intact.
        REQUIRE(copy.getMagnitudes().size() == 2);
        REQUIRE(copy.getPreferredMagnitude()->getType()
                == IMagnitude::Type::Local);
    }
    SECTION("Move")
    {
        Event toMove;
        toMove.setIdentifier(2024);
        toMove.setOrigins(std::vector<Origin> {makeValidOrigin()});
        toMove.setMagnitudes(makeMagnitudes());
        const Event moved{std::move(toMove)};
        REQUIRE(moved.getIdentifier() == 2024);
        REQUIRE(moved.getPreferredOrigin().hasLatitude());
        REQUIRE(moved.getMagnitudes().size() == 2);
        REQUIRE(moved.getPreferredMagnitude()->getType()
                == IMagnitude::Type::Local);
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::PeakToPeakAmplitude",
          "PeakToPeakAmplitude")
{
    SECTION("Defaults")
    {
        const PeakToPeakAmplitude amplitude;
        REQUIRE_FALSE(amplitude.hasStreamIdentifier());
        REQUIRE_FALSE(amplitude.hasPeakTimes());
        REQUIRE_FALSE(amplitude.hasAmplitude());
        REQUIRE_THROWS_AS(amplitude.getStreamIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(amplitude.getPeakTimes(), std::runtime_error);
        REQUIRE_THROWS_AS(amplitude.getAmplitude(), std::runtime_error);
    }
    SECTION("Amplitude is normalized to millimeters")
    {
        PeakToPeakAmplitude amplitude;
        amplitude.setAmplitude(2.0, PeakToPeakAmplitude::Units::Meters);
        REQUIRE(amplitude.getAmplitude() == Catch::Approx(2000.0));
        amplitude.setAmplitude(2.0, PeakToPeakAmplitude::Units::Centimeters);
        REQUIRE(amplitude.getAmplitude() == Catch::Approx(20.0));
        amplitude.setAmplitude(2.0, PeakToPeakAmplitude::Units::Millimeters);
        REQUIRE(amplitude.getAmplitude() == Catch::Approx(2.0));
    }
    SECTION("Non-positive amplitude throws")
    {
        PeakToPeakAmplitude amplitude;
        REQUIRE_THROWS_AS(
            amplitude.setAmplitude(0.0, PeakToPeakAmplitude::Units::Millimeters),
            std::invalid_argument);
        REQUIRE_THROWS_AS(
            amplitude.setAmplitude(-1.0, PeakToPeakAmplitude::Units::Meters),
            std::invalid_argument);
        REQUIRE_FALSE(amplitude.hasAmplitude());
    }
    SECTION("Stream identifier and peak times round trip")
    {
        constexpr std::chrono::nanoseconds firstPeak{100};
        constexpr std::chrono::nanoseconds secondPeak{250};
        PeakToPeakAmplitude amplitude;
        amplitude.setStreamIdentifier(makeStream("HHN"));
        const std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>
            peakTimes{firstPeak, secondPeak};
        amplitude.setPeakTimes(peakTimes);

        REQUIRE(amplitude.hasStreamIdentifier());
        REQUIRE(amplitude.getStreamIdentifier().getChannel() == "HHN");
        REQUIRE(amplitude.hasPeakTimes());
        REQUIRE(amplitude.getPeakTimes().first == firstPeak);
        REQUIRE(amplitude.getPeakTimes().second == secondPeak);
    }
    SECTION("Copy is a deep, independent copy")
    {
        auto amplitude = makeAmplitude("HHN", 6.434);
        const PeakToPeakAmplitude copy{amplitude};
        amplitude.setAmplitude(1.0, PeakToPeakAmplitude::Units::Millimeters);
        REQUIRE(copy.getAmplitude() == Catch::Approx(6.434));
        REQUIRE(copy.getStreamIdentifier().getChannel() == "HHN");
    }
    SECTION("Move")
    {
        auto toMove = makeAmplitude("HHE", 3.180);
        const PeakToPeakAmplitude moved{std::move(toMove)};
        REQUIRE(moved.getAmplitude() == Catch::Approx(3.180));
        REQUIRE(moved.getStreamIdentifier().getChannel() == "HHE");
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::StationLocalMagnitude",
          "StationLocalMagnitude")
{
    SECTION("Defaults")
    {
        const StationLocalMagnitude magnitude;
        REQUIRE_FALSE(magnitude.hasPeakToPeakAmplitudes());
        REQUIRE_FALSE(magnitude.hasWeight());
        REQUIRE_THROWS_AS(magnitude.getPeakToPeakAmplitudes(),
                          std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getWeight(), std::runtime_error);
    }
    SECTION("A valid two-channel amplitude pair round trips")
    {
        StationLocalMagnitude magnitude;
        magnitude.setPeakToPeakAmplitudes(
            {makeAmplitude("HHN", 6.434), makeAmplitude("HHE", 3.180)});
        REQUIRE(magnitude.hasPeakToPeakAmplitudes());
        const auto amplitudes = magnitude.getPeakToPeakAmplitudes();
        REQUIRE(amplitudes.first.getStreamIdentifier().getChannel() == "HHN");
        REQUIRE(amplitudes.second.getStreamIdentifier().getChannel() == "HHE");
    }
    SECTION("Two amplitudes from the same stream are rejected")
    {
        StationLocalMagnitude magnitude;
        REQUIRE_THROWS_AS(
            magnitude.setPeakToPeakAmplitudes(
                {makeAmplitude("HHN", 6.434), makeAmplitude("HHN", 3.180)}),
            std::invalid_argument);
    }
    SECTION("Amplitudes missing required fields are rejected")
    {
        StationLocalMagnitude magnitude;

        PeakToPeakAmplitude noStream;
        noStream.setAmplitude(1.0, PeakToPeakAmplitude::Units::Millimeters);
        REQUIRE_THROWS_AS(
            magnitude.setPeakToPeakAmplitudes(
                {noStream, makeAmplitude("HHE", 3.180)}),
            std::invalid_argument);

        PeakToPeakAmplitude noPeakTimes;
        noPeakTimes.setStreamIdentifier(makeStream("HHN"));
        noPeakTimes.setAmplitude(1.0, PeakToPeakAmplitude::Units::Millimeters);
        REQUIRE_THROWS_AS(
            magnitude.setPeakToPeakAmplitudes(
                {noPeakTimes, makeAmplitude("HHE", 3.180)}),
            std::invalid_argument);

        PeakToPeakAmplitude noAmplitude;
        noAmplitude.setStreamIdentifier(makeStream("HHN"));
        const std::pair<std::chrono::nanoseconds, std::chrono::nanoseconds>
            peakTimes{std::chrono::nanoseconds{1}, std::chrono::nanoseconds{2}};
        noAmplitude.setPeakTimes(peakTimes);
        REQUIRE_THROWS_AS(
            magnitude.setPeakToPeakAmplitudes(
                {noAmplitude, makeAmplitude("HHE", 3.180)}),
            std::invalid_argument);
    }
    SECTION("Weight bounds")
    {
        StationLocalMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setWeight(-0.1), std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setWeight(1.1), std::invalid_argument);
        REQUIRE_NOTHROW(magnitude.setWeight(0.0));
        REQUIRE_NOTHROW(magnitude.setWeight(1.0));
        REQUIRE(magnitude.getWeight() == 1.0);
    }
    SECTION("Copy is a deep, independent copy")
    {
        StationLocalMagnitude magnitude;
        magnitude.setPeakToPeakAmplitudes(
            {makeAmplitude("HHN", 6.434), makeAmplitude("HHE", 3.180)});
        magnitude.setWeight(1.0);

        const StationLocalMagnitude copy{magnitude};
        magnitude.setWeight(0.0);
        REQUIRE(magnitude.getWeight() == 0.0);
        REQUIRE(copy.getWeight() == 1.0);
        REQUIRE(copy.hasPeakToPeakAmplitudes());
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::StationDurationMagnitude",
          "StationDurationMagnitude")
{
    SECTION("Defaults")
    {
        const StationDurationMagnitude magnitude;
        REQUIRE_FALSE(magnitude.hasDuration());
        REQUIRE_FALSE(magnitude.hasDistance());
        REQUIRE_FALSE(magnitude.hasResidual());
        REQUIRE_FALSE(magnitude.hasWeight());
        REQUIRE(magnitude.getCorrection() == 0.0);   // Default, not "has".
        REQUIRE(magnitude.getDistance() == 0.0);      // noexcept: 0 when unset.
        REQUIRE_THROWS_AS(magnitude.getDuration(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getResidual(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getWeight(), std::runtime_error);
    }
    SECTION("Duration must be positive")
    {
        StationDurationMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setDuration(0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setDuration(-1.0), std::invalid_argument);
        magnitude.setDuration(12.5);
        REQUIRE(magnitude.hasDuration());
        REQUIRE(magnitude.getDuration() == 12.5);
    }
    SECTION("Distance cannot be negative")
    {
        StationDurationMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setDistance(-1.0), std::invalid_argument);
        magnitude.setDistance(177000.0);
        REQUIRE(magnitude.hasDistance());
        REQUIRE(magnitude.getDistance() == 177000.0);
    }
    SECTION("Correction and residual")
    {
        StationDurationMagnitude magnitude;
        magnitude.setCorrection(-0.55);
        REQUIRE(magnitude.getCorrection() == -0.55);
        magnitude.setResidual(0.22);
        REQUIRE(magnitude.hasResidual());
        REQUIRE(magnitude.getResidual() == 0.22);
    }
    SECTION("Weight bounds")
    {
        StationDurationMagnitude magnitude;
        REQUIRE_THROWS_AS(magnitude.setWeight(-0.1), std::invalid_argument);
        REQUIRE_THROWS_AS(magnitude.setWeight(1.1), std::invalid_argument);
        REQUIRE_NOTHROW(magnitude.setWeight(1.0));
        REQUIRE(magnitude.getWeight() == 1.0);
    }
    SECTION("Copy is a deep, independent copy")
    {
        StationDurationMagnitude magnitude;
        magnitude.setDuration(12.5);
        magnitude.setDistance(177000.0);

        const StationDurationMagnitude copy{magnitude};
        magnitude.setDuration(20.0);
        REQUIRE(magnitude.getDuration() == 20.0);
        REQUIRE(copy.getDuration() == 12.5);
        REQUIRE(copy.getDistance() == 177000.0);
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::NetworkMagnitudeStations",
          "NetworkMagnitudeStations")
{
    SECTION("LocalMagnitude aggregates station magnitudes")
    {
        StationLocalMagnitude station;
        station.setPeakToPeakAmplitudes(
            {makeAmplitude("HHN", 6.434), makeAmplitude("HHE", 3.180)});
        station.setWeight(1.0);

        LocalMagnitude magnitude;
        magnitude.setValue(3.23);
        magnitude.setStationMagnitudes(
            std::vector<StationLocalMagnitude> {station, station});
        REQUIRE(magnitude.getStationMagnitudes().size() == 2);
        REQUIRE(magnitude.getStationMagnitudes().at(0).getWeight() == 1.0);
        // The base magnitude value is untouched by the station list.
        REQUIRE(magnitude.getValue() == 3.23);
    }
    SECTION("LocalMagnitude station magnitudes can be moved in")
    {
        StationLocalMagnitude station;
        station.setPeakToPeakAmplitudes(
            {makeAmplitude("HHN", 6.434), makeAmplitude("HHE", 3.180)});
        std::vector<StationLocalMagnitude> stations{station, station, station};

        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::move(stations));
        REQUIRE(magnitude.getStationMagnitudes().size() == 3);
    }
    SECTION("DurationMagnitude aggregates station magnitudes")
    {
        StationDurationMagnitude station;
        station.setDuration(12.5);
        station.setDistance(177000.0);

        DurationMagnitude magnitude;
        magnitude.setValue(2.9);
        magnitude.setStationMagnitudes(
            std::vector<StationDurationMagnitude> {station, station});
        REQUIRE(magnitude.getStationMagnitudes().size() == 2);
        REQUIRE(magnitude.getStationMagnitudes().at(1).getDuration() == 12.5);
        REQUIRE(magnitude.getValue() == 2.9);
    }
}

namespace
{

/// @brief Builds a valid arrival on its own channel at the given time.
/// @note A distinct channel per arrival keeps a set of them clear of the
///       duplicate stream-and-phase check in Origin::setArrivals, which is
///       not what these tests are about.
Arrival makeArrivalAt(const int64_t identifier,
                      const std::string &channel,
                      const std::chrono::seconds &time,
                      const Arrival::Phase phase = Arrival::Phase::P)
{
    Arrival arrival;
    arrival.setIdentifier(identifier);
    arrival.setPhase(phase);
    arrival.setStreamIdentifier(makeStream(channel));
    arrival.setTime(time);
    arrival.setReviewStatus(Arrival::ReviewStatus::Automatic);
    return arrival;
}

/// @brief An origin holding three arrivals, handed to setArrivals OUT of
///        time order so the ordering the class imposes is visible.
Origin makeOriginWithArrivals()
{
    auto origin = makeValidOrigin();
    origin.setArrivals(std::vector<Arrival> {
        makeArrivalAt(3, "HHZ", std::chrono::seconds {300}),
        makeArrivalAt(1, "HHN", std::chrono::seconds {100}),
        makeArrivalAt(2, "HHE", std::chrono::seconds {200})});
    return origin;
}

}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::Origin iterators", "Origin")
{
    SECTION("An origin with no arrivals iterates over nothing")
    {
        Origin origin;
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


namespace
{

/// @brief Builds a station local magnitude with a valid two-channel
///        amplitude pair and the given weight.
StationLocalMagnitude makeStationLocalMagnitude(const double weight,
                                                const double amplitude)
{
    StationLocalMagnitude magnitude;
    magnitude.setPeakToPeakAmplitudes(
        {makeAmplitude("HHN", amplitude), makeAmplitude("HHE", amplitude/2)});
    magnitude.setWeight(weight);
    return magnitude;
}

/// @brief Builds a station duration magnitude with the given weight.
StationDurationMagnitude makeStationDurationMagnitude(const double weight,
                                                      const double duration)
{
    StationDurationMagnitude magnitude;
    magnitude.setDuration(duration);
    magnitude.setWeight(weight);
    return magnitude;
}

}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::LocalMagnitude iterators",
          "LocalMagnitude")
{
    SECTION("A magnitude with no station magnitudes iterates over nothing")
    {
        const LocalMagnitude magnitude;
        REQUIRE(magnitude.size() == 0);
        REQUIRE(magnitude.begin() == magnitude.end());
        REQUIRE(magnitude.cbegin() == magnitude.cend());
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 0);

        int visited{0};
        for ([[maybe_unused]] const auto &station : magnitude){++visited;}
        REQUIRE(visited == 0);
    }
    SECTION("Iteration visits every station magnitude in insertion order")
    {
        // Unlike Origin::setArrivals, setStationMagnitudes neither sorts
        // nor validates - what goes in is what comes back, in order.
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0),
            makeStationLocalMagnitude(0.0, 2.0)});

        REQUIRE(magnitude.size() == 3);
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 3);

        std::vector<double> weights;
        for (const auto &station : magnitude)
        {
            weights.push_back(station.getWeight());
        }
        REQUIRE(weights == std::vector<double> {1.0, 0.5, 0.0});
    }
    SECTION("size, distance and getStationMagnitudes agree")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        const auto stations = magnitude.getStationMagnitudes();
        REQUIRE(stations.size() == magnitude.size());
        REQUIRE(static_cast<std::size_t>
                (std::distance(magnitude.begin(), magnitude.end()))
                == magnitude.size());
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            REQUIRE(stations.at(i).getWeight() == magnitude.at(i).getWeight());
        }
    }
    SECTION("at, operator[] and the iterator name the same object")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            const auto offset = static_cast<std::ptrdiff_t> (i);
            REQUIRE(&magnitude.at(i) == &magnitude[i]);
            REQUIRE(&magnitude.at(i)
                    == &*std::next(magnitude.begin(), offset));
        }
    }
    SECTION("at is bounds checked")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0)});
        REQUIRE_THROWS_AS(magnitude.at(1), std::out_of_range);

        const LocalMagnitude empty;
        REQUIRE_THROWS_AS(empty.at(0), std::out_of_range);
    }
    SECTION("Access is read-only even through a non-const magnitude")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0)});
        static_assert(std::is_same_v<LocalMagnitude::iterator,
                                     LocalMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(magnitude.begin()),
                                     LocalMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(*magnitude.begin()),
                                     const StationLocalMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude.at(0)),
                                     const StationLocalMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude[0]),
                                     const StationLocalMagnitude &>);
        REQUIRE(magnitude.begin()->getWeight() == 1.0);
    }
    SECTION("Standard algorithms work over the range")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0),
            makeStationLocalMagnitude(0.0, 2.0)});
        REQUIRE(std::count_if(magnitude.begin(), magnitude.end(),
                              [](const StationLocalMagnitude &station)
                              {
                                  return station.getWeight() > 0.0;
                              }) == 2);
        const auto found
            = std::find_if(magnitude.begin(), magnitude.end(),
                           [](const StationLocalMagnitude &station)
                           {
                               return station.getWeight() == 0.5;
                           });
        REQUIRE(found != magnitude.end());
        REQUIRE(std::distance(magnitude.begin(), found) == 1);
    }
    SECTION("A copy iterates over its own station magnitudes")
    {
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        LocalMagnitude copy{magnitude};
        REQUIRE(copy.size() == magnitude.size());
        REQUIRE(&*copy.begin() != &*magnitude.begin());

        copy.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(0.25, 1.0)});
        REQUIRE(copy.size() == 1);
        REQUIRE(magnitude.size() == 2);
        REQUIRE(magnitude.begin()->getWeight() == 1.0);
    }
    SECTION("The station magnitudes survive a clone through the base")
    {
        // clone() returns unique_ptr<IMagnitude>, which has no iterators;
        // this checks the payload is carried across all the same.
        LocalMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationLocalMagnitude> {
            makeStationLocalMagnitude(1.0, 6.0),
            makeStationLocalMagnitude(0.5, 4.0)});
        const auto cloned = magnitude.clone();
        REQUIRE(cloned != nullptr);
        const auto *asLocal
            = dynamic_cast<const LocalMagnitude *> (cloned.get());
        REQUIRE(asLocal != nullptr);
        REQUIRE(asLocal->size() == 2);
        REQUIRE(std::distance(asLocal->begin(), asLocal->end()) == 2);
        REQUIRE(asLocal->begin()->getWeight() == 1.0);
    }
}

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::DurationMagnitude iterators",
          "DurationMagnitude")
{
    SECTION("A magnitude with no station magnitudes iterates over nothing")
    {
        const DurationMagnitude magnitude;
        REQUIRE(magnitude.size() == 0);
        REQUIRE(magnitude.begin() == magnitude.end());
        REQUIRE(magnitude.cbegin() == magnitude.cend());
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 0);

        int visited{0};
        for ([[maybe_unused]] const auto &station : magnitude){++visited;}
        REQUIRE(visited == 0);
    }
    SECTION("Iteration visits every station magnitude in insertion order")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0),
            makeStationDurationMagnitude(0.0, 10.0)});

        REQUIRE(magnitude.size() == 3);
        REQUIRE(std::distance(magnitude.begin(), magnitude.end()) == 3);

        std::vector<double> durations;
        for (const auto &station : magnitude)
        {
            durations.push_back(station.getDuration());
        }
        REQUIRE(durations == std::vector<double> {30.0, 20.0, 10.0});
    }
    SECTION("size, distance and getStationMagnitudes agree")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        const auto stations = magnitude.getStationMagnitudes();
        REQUIRE(stations.size() == magnitude.size());
        REQUIRE(static_cast<std::size_t>
                (std::distance(magnitude.begin(), magnitude.end()))
                == magnitude.size());
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            REQUIRE(stations.at(i).getDuration()
                    == magnitude.at(i).getDuration());
        }
    }
    SECTION("at, operator[] and the iterator name the same object")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        for (std::size_t i = 0; i < magnitude.size(); ++i)
        {
            const auto offset = static_cast<std::ptrdiff_t> (i);
            REQUIRE(&magnitude.at(i) == &magnitude[i]);
            REQUIRE(&magnitude.at(i)
                    == &*std::next(magnitude.begin(), offset));
        }
    }
    SECTION("at is bounds checked")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0)});
        REQUIRE_THROWS_AS(magnitude.at(1), std::out_of_range);

        const DurationMagnitude empty;
        REQUIRE_THROWS_AS(empty.at(0), std::out_of_range);
    }
    SECTION("Access is read-only even through a non-const magnitude")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0)});
        static_assert(std::is_same_v<DurationMagnitude::iterator,
                                     DurationMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(magnitude.begin()),
                                     DurationMagnitude::const_iterator>);
        static_assert(std::is_same_v<decltype(*magnitude.begin()),
                                     const StationDurationMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude.at(0)),
                                     const StationDurationMagnitude &>);
        static_assert(std::is_same_v<decltype(magnitude[0]),
                                     const StationDurationMagnitude &>);
        REQUIRE(magnitude.begin()->getDuration() == 30.0);
    }
    SECTION("Standard algorithms work over the range")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0),
            makeStationDurationMagnitude(0.0, 10.0)});
        REQUIRE(std::count_if(magnitude.begin(), magnitude.end(),
                              [](const StationDurationMagnitude &station)
                              {
                                  return station.getWeight() > 0.0;
                              }) == 2);
        const auto longest
            = std::max_element(magnitude.begin(), magnitude.end(),
                               [](const StationDurationMagnitude &lhs,
                                  const StationDurationMagnitude &rhs)
                               {
                                   return lhs.getDuration()
                                        < rhs.getDuration();
                               });
        REQUIRE(longest == magnitude.begin());
    }
    SECTION("A copy iterates over its own station magnitudes")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        DurationMagnitude copy{magnitude};
        REQUIRE(copy.size() == magnitude.size());
        REQUIRE(&*copy.begin() != &*magnitude.begin());

        copy.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(0.25, 5.0)});
        REQUIRE(copy.size() == 1);
        REQUIRE(magnitude.size() == 2);
        REQUIRE(magnitude.begin()->getDuration() == 30.0);
    }
    SECTION("The station magnitudes survive a clone through the base")
    {
        DurationMagnitude magnitude;
        magnitude.setStationMagnitudes(std::vector<StationDurationMagnitude> {
            makeStationDurationMagnitude(1.0, 30.0),
            makeStationDurationMagnitude(0.5, 20.0)});
        const auto cloned = magnitude.clone();
        REQUIRE(cloned != nullptr);
        const auto *asDuration
            = dynamic_cast<const DurationMagnitude *> (cloned.get());
        REQUIRE(asDuration != nullptr);
        REQUIRE(asDuration->size() == 2);
        REQUIRE(std::distance(asDuration->begin(), asDuration->end()) == 2);
        REQUIRE(asDuration->begin()->getDuration() == 30.0);
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

        Event event;
        event.setIdentifier(77);
        event.setOrigins(std::vector<Origin> {origin1, origin2});
        event.setMagnitudes(makeMagnitudes());
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

        const auto magnitudes = event.magnitudes();
        REQUIRE(magnitudes.size() == 2);
        REQUIRE(event.magnitudes().data() == magnitudes.data());
        REQUIRE(magnitudes[0].get() == &event.preferredMagnitude());
    }
    SECTION("The copying getters really do copy")
    {
        // The contrast that justifies the views existing at all.
        const auto event = makeEvent();
        const auto copied = event.getOrigins();
        REQUIRE(copied.size() == event.origins().size());
        REQUIRE(copied.data() != event.origins().data());

        const auto clonedMagnitudes = event.getMagnitudes();
        REQUIRE(clonedMagnitudes.size() == event.magnitudes().size());
        REQUIRE(clonedMagnitudes[0].get() != event.magnitudes()[0].get());
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
        for (const auto &magnitude : event.magnitudes())
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
        REQUIRE(std::count_if(event.magnitudes().begin(),
                              event.magnitudes().end(),
                              [](const std::unique_ptr<IMagnitude> &magnitude)
                              {
                                  return magnitude->isPreferred();
                              }) == 1);
    }
    SECTION("An empty event has nothing to view")
    {
        const Event event;
        REQUIRE_THROWS_AS(event.origins(), std::runtime_error);
        REQUIRE_THROWS_AS(event.magnitudes(), std::runtime_error);
        REQUIRE_THROWS_AS(event.preferredOrigin(), std::runtime_error);
        REQUIRE_THROWS_AS(event.preferredMagnitude(), std::runtime_error);
    }
    SECTION("A view of a copied event is that copy's own storage")
    {
        const auto event = makeEvent();
        const Event copy{event};
        REQUIRE(copy.origins().size() == event.origins().size());
        REQUIRE(copy.origins().data() != event.origins().data());
        REQUIRE(copy.magnitudes()[0].get() != event.magnitudes()[0].get());
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

TEST_CASE("AQMSDutyReviewBackend::Database::AQMS::CentroidMomentTensorMagnitude",
          "CentroidMomentTensorMagnitude")
{
    // A CMT magnitude is computed out of band from AQMS, so this class
    // carries the punchline and nothing else: no moment tensor, no
    // variance reduction, no measure of waveform fit.  The value just is.
    // Everything here is therefore the base interface - if this test ever
    // needs to reach for something CMT-specific, the class has grown a
    // surface it was deliberately built without.
    SECTION("Defaults")
    {
        const CentroidMomentTensorMagnitude magnitude;
        REQUIRE(magnitude.getType() == IMagnitude::Type::Moment);
        REQUIRE(magnitude.isPreferred());
        REQUIRE_FALSE(magnitude.hasIdentifier());
        REQUIRE_FALSE(magnitude.hasValue());
        REQUIRE_THROWS_AS(magnitude.getIdentifier(), std::runtime_error);
        REQUIRE_THROWS_AS(magnitude.getValue(), std::runtime_error);
        // Reviewed out of the box, with nothing set: nobody auto-computes
        // a CMT, so that is the sensible default when a row says nothing.
        REQUIRE(magnitude.hasReviewStatus());
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("The review status defaults to human reviewed")
    {
        // Nobody auto-computes a CMT, so a row that says nothing means a
        // person ran another application, decided the answer was good,
        // and wrote the values in.
        const CentroidMomentTensorMagnitude magnitude;
        REQUIRE(magnitude.hasReviewStatus());
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("The database's answer wins, however odd")
    {
        // An automatic CMT should not exist, but if AQMS holds one this
        // is a view of AQMS and reports it.  The oddity gets logged where
        // the row is read; it does not get laundered here.
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Automatic);
        REQUIRE(magnitude.hasReviewStatus());
        REQUIRE(magnitude.getReviewStatus()
                == IMagnitude::ReviewStatus::Automatic);

        // And back again, so the setter is not one-way.
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Human);
        REQUIRE(magnitude.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("The database's answer survives a copy, move and clone")
    {
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setValue(5.7);
        magnitude.setReviewStatus(IMagnitude::ReviewStatus::Automatic);

        const CentroidMomentTensorMagnitude copy{magnitude};
        REQUIRE(copy.getReviewStatus() == IMagnitude::ReviewStatus::Automatic);

        const auto cloned = magnitude.clone();
        REQUIRE(cloned->getReviewStatus()
                == IMagnitude::ReviewStatus::Automatic);

        const CentroidMomentTensorMagnitude moved{std::move(magnitude)};
        REQUIRE(moved.getReviewStatus() == IMagnitude::ReviewStatus::Automatic);
    }
    SECTION("Set and get through the base interface")
    {
        constexpr int64_t identifier{451};
        constexpr double value{5.7};
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setIdentifier(identifier);
        magnitude.setValue(value);
        magnitude.setNotPreferred();

        REQUIRE(magnitude.getIdentifier() == identifier);
        REQUIRE(magnitude.getValue() == value);
        REQUIRE(magnitude.hasIdentifier());
        REQUIRE(magnitude.hasValue());
        REQUIRE_FALSE(magnitude.isPreferred());

        magnitude.setIsPreferred();
        REQUIRE(magnitude.isPreferred());
    }
    SECTION("Value bounds are the base class's")
    {
        CentroidMomentTensorMagnitude magnitude;
        REQUIRE_NOTHROW(magnitude.setValue(11.0));
        REQUIRE(magnitude.getValue() == 11.0);
        REQUIRE_NOTHROW(magnitude.setValue(-1.5));
        REQUIRE(magnitude.getValue() == -1.5);
        REQUIRE_THROWS_AS(magnitude.setValue(11.0001), std::invalid_argument);
    }
    SECTION("Clone preserves the derived type through a base pointer")
    {
        CentroidMomentTensorMagnitude moment;
        moment.setIdentifier(12);
        moment.setValue(5.1);
        // Held as a base pointer, which is how Event stores it.
        const std::unique_ptr<IMagnitude> base
            = std::make_unique<CentroidMomentTensorMagnitude> (moment);
        const auto cloned = base->clone();
        REQUIRE(cloned != nullptr);
        REQUIRE(cloned.get() != base.get());
        REQUIRE(cloned->getType() == IMagnitude::Type::Moment);
        REQUIRE(cloned->getIdentifier() == 12);
        REQUIRE(cloned->getValue() == 5.1);
        REQUIRE(cloned->hasReviewStatus());
        REQUIRE(cloned->getReviewStatus() == IMagnitude::ReviewStatus::Human);
        REQUIRE(dynamic_cast<const CentroidMomentTensorMagnitude *>
                (cloned.get()) != nullptr);
    }
    SECTION("Copy preserves base state independently")
    {
        CentroidMomentTensorMagnitude magnitude;
        magnitude.setIdentifier(21);
        magnitude.setValue(4.4);
        magnitude.setNotPreferred();

        const CentroidMomentTensorMagnitude copy{magnitude};
        magnitude.setValue(6.6);
        REQUIRE(magnitude.getValue() == 6.6);
        REQUIRE(copy.getType() == IMagnitude::Type::Moment);
        REQUIRE(copy.getIdentifier() == 21);
        REQUIRE(copy.getValue() == 4.4);
        REQUIRE_FALSE(copy.isPreferred());
    }
    SECTION("Move preserves base state and type")
    {
        CentroidMomentTensorMagnitude toMove;
        toMove.setIdentifier(31);
        toMove.setValue(6.2);
        const CentroidMomentTensorMagnitude moved{std::move(toMove)};
        REQUIRE(moved.getType() == IMagnitude::Type::Moment);
        REQUIRE(moved.getIdentifier() == 31);
        REQUIRE(moved.getValue() == 6.2);
        REQUIRE(moved.hasReviewStatus());
        REQUIRE(moved.getReviewStatus() == IMagnitude::ReviewStatus::Human);
    }
    SECTION("Moment is its own type alongside the others")
    {
        // Event rejects two magnitudes of the same type, so this being
        // distinct is what lets a CMT sit next to an Ml and an Md.
        REQUIRE(CentroidMomentTensorMagnitude{}.getType()
                != HumanMagnitude{}.getType());
        REQUIRE(CentroidMomentTensorMagnitude{}.getType()
                != LocalMagnitude{}.getType());
        REQUIRE(CentroidMomentTensorMagnitude{}.getType()
                != DurationMagnitude{}.getType());

        auto local = std::make_unique<LocalMagnitude> ();
        local->setValue(3.4);
        local->setNotPreferred();
        auto moment = std::make_unique<CentroidMomentTensorMagnitude> ();
        moment->setValue(5.7);
        moment->setIsPreferred();

        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.push_back(std::move(local));
        magnitudes.push_back(std::move(moment));

        Event event;
        REQUIRE_NOTHROW(event.setMagnitudes(magnitudes));
        REQUIRE(event.magnitudes().size() == 2);
        REQUIRE(event.preferredMagnitude().getType()
                == IMagnitude::Type::Moment);
        REQUIRE(event.preferredMagnitude().getValue() == 5.7);
    }
    SECTION("Two moment magnitudes in one event are rejected")
    {
        auto first = std::make_unique<CentroidMomentTensorMagnitude> ();
        first->setValue(5.7);
        first->setIsPreferred();
        auto second = std::make_unique<CentroidMomentTensorMagnitude> ();
        second->setValue(5.9);
        second->setNotPreferred();

        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.push_back(std::move(first));
        magnitudes.push_back(std::move(second));

        Event event;
        REQUIRE_THROWS_AS(event.setMagnitudes(magnitudes),
                          std::invalid_argument);
    }
}

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

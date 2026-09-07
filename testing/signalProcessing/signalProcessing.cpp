#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "aqmsDutyReviewBackend/signalProcessing/bandpass.hpp"
#include "aqmsDutyReviewBackend/signalProcessing/demean.hpp"

using namespace AQMSDutyReviewBackend::SignalProcessing;

namespace
{

/// Everything here is compared against scipy, so the tolerance is about
/// the last few bits of a double surviving a different order of
/// operations - not about the filters being approximately right.
constexpr double tolerance{1.e-8};

/// @brief Loads one of design.py's reference impulse responses.
/// @note These are scipy's sosfilt applied to a unit impulse, so comparing
///       against them checks the coefficients AND the way the sections are
///       chained, which is where an implementation actually goes wrong.
[[nodiscard]] std::vector<double> loadReference(const int samplingRate)
{
    const auto path
        = std::filesystem::path {TESTING_DATA_DIR}/"signalProcessing"
        / ("bandpassReferenceFS-" + std::to_string(samplingRate) + ".txt");
    if (!std::filesystem::exists(path))
    {
        throw std::runtime_error("Reference " + std::string {path}
                               + " does not exist");
    }
    std::ifstream file{path};
    std::vector<double> result;
    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty()){continue;}
        result.push_back(std::stod(line));
    }
    return result;
}

/// @brief A unit impulse - the input design.py filtered to make the
///        references.
[[nodiscard]] std::vector<double> makeImpulse(const std::size_t nSamples)
{
    std::vector<double> impulse(nSamples, 0.0);
    impulse.at(0) = 1.0;
    return impulse;
}

}

/// The impulse response is the filter.  Two filters agree on every input
/// if and only if they agree here, so matching scipy's sosfilt sample for
/// sample is the whole of the check.
TEST_CASE("AQMSDutyReviewBackend::SignalProcessing::bandpassFilter",
          "[signalProcessing]")
{
    SECTION("The impulse response matches scipy at every sampling rate")
    {
        for (const auto samplingRate : {40, 80, 100, 200, 500, 1000})
        {
            CAPTURE(samplingRate);
            const auto reference = ::loadReference(samplingRate);
            REQUIRE(reference.size() == 1000);

            const auto response
                = bandpassFilter(::makeImpulse(reference.size()),
                                 samplingRate);
            REQUIRE(response.size() == reference.size());
            for (std::size_t i = 0; i < reference.size(); ++i)
            {
                CAPTURE(i);
                REQUIRE(std::abs(response.at(i) - reference.at(i))
                        < ::tolerance);
            }
        }
    }
    SECTION("The response is not trivially zero or a copy of the input")
    {
        // Guards the comparison above: an implementation returning the
        // input unchanged, or nothing at all, would sail through a check
        // that only asked whether two arrays were close if the reference
        // were also degenerate.
        const auto response = bandpassFilter(::makeImpulse(1000), 100);
        const auto energy
            = std::accumulate(response.begin(), response.end(), 0.0,
                              [](const double total, const double sample)
                              {
                                  return total + sample*sample;
                              });
        REQUIRE(energy > 0.1);
        REQUIRE(response.at(0) != Catch::Approx(1.0));
    }
    SECTION("An empty signal comes back empty rather than throwing")
    {
        REQUIRE(bandpassFilter(std::vector<double> {}, 100).empty());
        // Even at a rate that has no design - there is nothing to filter,
        // so there is nothing to refuse.
        REQUIRE(bandpassFilter(std::vector<double> {}, 12345).empty());
    }
    SECTION("A sampling rate with no design is refused")
    {
        REQUIRE_THROWS_AS(bandpassFilter(::makeImpulse(10), 250),
                          std::runtime_error);
        REQUIRE_THROWS_AS(bandpassFilter(::makeImpulse(10), 0),
                          std::runtime_error);
    }
    SECTION("A constant signal is very nearly removed")
    {
        // The passband starts at 0.5 Hz, so a DC signal is exactly what
        // this filter is meant to reject.  What survives is the start-up
        // transient, which decays.
        const std::vector<double> constant(1000, 1.0);
        const auto filtered = bandpassFilter(constant, 100);
        REQUIRE(filtered.size() == constant.size());
        for (std::size_t i = 500; i < filtered.size(); ++i)
        {
            CAPTURE(i);
            REQUIRE(std::abs(filtered.at(i)) < 1.e-3);
        }
    }
}

/// The packets' sampling rates are not exactly the rates the filters were
/// designed at, so this is what decides which design a segment gets.
TEST_CASE("AQMSDutyReviewBackend::SignalProcessing::getValidSamplingRate",
          "[signalProcessing]")
{
    SECTION("An exact rate maps to itself")
    {
        for (const auto samplingRate : {40, 80, 100, 200, 500, 1000})
        {
            CAPTURE(samplingRate);
            REQUIRE(getValidSamplingRate(samplingRate) == samplingRate);
        }
    }
    SECTION("A rate slightly off still finds its design")
    {
        // The point of the function: a packet reporting 99.9999 Hz is a
        // 100 Hz channel, and refusing it would leave the trace unfiltered.
        REQUIRE(getValidSamplingRate(100.0005) == 100);
        REQUIRE(getValidSamplingRate(99.9995) == 100);
        REQUIRE(getValidSamplingRate(40.00005) == 40);
        REQUIRE(getValidSamplingRate(1000.005) == 1000);
    }
    SECTION("A rate too far off is refused rather than rounded")
    {
        // Snapping 90 Hz onto the 100 Hz design would apply a filter whose
        // corners sit in the wrong place, which is worse than not
        // filtering: the trace would look plausible and be wrong.
        REQUIRE_THROWS_AS(getValidSamplingRate(90.0), std::runtime_error);
        REQUIRE_THROWS_AS(getValidSamplingRate(250.0), std::runtime_error);
        REQUIRE_THROWS_AS(getValidSamplingRate(101.0), std::runtime_error);
    }
    SECTION("A non-positive rate is a bad argument, not a missing design")
    {
        REQUIRE_THROWS_AS(getValidSamplingRate(0.0), std::invalid_argument);
        REQUIRE_THROWS_AS(getValidSamplingRate(-100.0),
                          std::invalid_argument);
    }
}

TEST_CASE("AQMSDutyReviewBackend::SignalProcessing::demean",
          "[signalProcessing]")
{
    SECTION("A constant signal is brought to zero")
    {
        const std::vector<double> ones(1000, 1.0);
        const auto demeaned = demean(ones);
        REQUIRE(demeaned.size() == ones.size());
        for (const auto sample : demeaned)
        {
            REQUIRE(std::abs(sample) < ::tolerance);
        }
    }
    SECTION("Only the offset moves - the shape is untouched")
    {
        // The property that matters for a trace: removing the mean must
        // not change what the wiggles look like, only where zero is.
        std::vector<double> signal(1000);
        for (std::size_t i = 0; i < signal.size(); ++i)
        {
            signal.at(i) = 500.0 + std::sin(0.05*static_cast<double> (i));
        }
        const auto demeaned = demean(signal);
        for (std::size_t i = 1; i < signal.size(); ++i)
        {
            CAPTURE(i);
            REQUIRE(std::abs((demeaned.at(i) - demeaned.at(i - 1))
                           - (signal.at(i) - signal.at(i - 1)))
                    < ::tolerance);
        }
    }
    SECTION("The result has zero mean, and doing it twice changes nothing")
    {
        std::vector<double> signal(1000);
        for (std::size_t i = 0; i < signal.size(); ++i)
        {
            signal.at(i) = 1000.0 + 3.0*static_cast<double> (i);
        }
        const auto demeaned = demean(signal);
        const auto mean
            = std::accumulate(demeaned.begin(), demeaned.end(), 0.0)
            / static_cast<double> (demeaned.size());
        REQUIRE(std::abs(mean) < ::tolerance);

        const auto again = demean(demeaned);
        for (std::size_t i = 0; i < again.size(); ++i)
        {
            CAPTURE(i);
            REQUIRE(std::abs(again.at(i) - demeaned.at(i)) < ::tolerance);
        }
    }
    SECTION("A single sample becomes zero")
    {
        const auto demeaned = demean(std::vector<double> {42.0});
        REQUIRE(demeaned.size() == 1);
        REQUIRE(std::abs(demeaned.at(0)) < ::tolerance);
    }
    SECTION("An empty signal is refused")
    {
        REQUIRE_THROWS_AS(demean(std::vector<double> {}),
                          std::invalid_argument);
    }
}

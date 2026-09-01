#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <pqxx/pqxx>
#include <libmseed.h>
#include "aqmsDutyReviewBackend/database/aqms/queries/waveformQueries.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"
#include "aqmsDutyReviewBackend/database/aqms/waveform.hpp"
#include "aqmsDutyReviewBackend/database/aqms/segment.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{

/// @brief Converts hex to ASCII
[[nodiscard]]
std::string hexToASCII(const std::string &hex, //const auto &hex, //const std::string &hex,
                       const int outputSize =-1,
                       const bool doul = true)
{
    std::string ascii;
    size_t length = hex.size();
    if (outputSize < 0)
    {   
        ascii.resize(hex.length()/2);
    }   
    else
    {   
        ascii.resize(outputSize);
        length = 2*outputSize; 
    }   
    const auto hexData = reinterpret_cast<const char *> (hex.data());
    // TODO can I just allocate 2 off the bet and use SSO?
    std::string part;
    part.resize(2); 
    size_t j{0};
    for (size_t i = 0; i < length; i += 2)
    {   
        part[0] = hexData[i + 0]; 
        part[1] = hexData[i + 1]; 
        // Change it into base 16 and typecast as the character
        constexpr int base{16};
        if (doul)
        {
            ascii[j] = std::stoul(part, nullptr, base);
        }
        else
        {
            ascii[j] = std::stoi(part.c_str(), nullptr, base);
        }
        // Add this char to final ASCII string
        j = j + 1;
    }   
    return ascii;
}

/// @brief Unpacks the waveform.  The result is a miniSEED "file" for this
///        particular network/station/channel/location code.
Waveform unpack(std::string &data,
                const size_t nBytes,
                const int8_t verbose,
                const bool purgeTrailingZeros)
{
    std::vector<Segment> segments;
    auto bufferSize = static_cast<uint64_t> (nBytes);
    if (data.size() != nBytes)
    {
        throw std::runtime_error("Inconsistent sizes");
    }
    StreamIdentifier streamIdentifier;
    uint64_t offset{0};
    while (bufferSize - offset > MINRECLEN)
    {
        constexpr uint32_t flags{MSF_UNPACKDATA};
        Segment segment;
        MS3Record *miniSEEDRecord{nullptr};
        auto returnCode = msr3_parse(data.c_str() + offset,
                                     static_cast<uint64_t> (bufferSize) - offset,
                                     &miniSEEDRecord, flags,
                                     verbose);
        if (returnCode == MS_NOERROR && miniSEEDRecord)
        {
            // SNCL
            std::array<char, 64> networkWork;
            std::array<char, 64> stationWork;
            std::array<char, 64> channelWork;
            std::array<char, 64> locationWork;
            std::fill(networkWork.begin(),  networkWork.end(), '\0');
            std::fill(stationWork.begin(),  stationWork.end(), '\0');
            std::fill(channelWork.begin(),  channelWork.end(), '\0');
            std::fill(locationWork.begin(), locationWork.end(), '\0');
            returnCode = ms_sid2nslc_n(miniSEEDRecord->sid,
                                       networkWork.data(), networkWork.size(),
                                       stationWork.data(), stationWork.size(),
                                       locationWork.data(), locationWork.size(),
                                       channelWork.data(), channelWork.size());
            std::string network{networkWork.data()};
            std::string station{stationWork.data()};
            std::string channel{channelWork.data()};
            std::string location{locationWork.data()};
            if (returnCode == MS_NOERROR)
            {
                if (!streamIdentifier.hasNetwork())
                {
                    streamIdentifier.setNetwork(network);
                    streamIdentifier.setStation(station);
                    streamIdentifier.setChannel(channel);
                    streamIdentifier.setLocationCode(location);
                }
                else
                {
                    if (network != streamIdentifier.getNetwork())
                    {
                        throw std::runtime_error("Inconsistent network");
                    }
                    if (station != streamIdentifier.getStation())
                    {
                        throw std::runtime_error("Inconsistent station");
                    }
                    if (channel != streamIdentifier.getChannel())
                    {
                        throw std::runtime_error("Inconsistent channel");
                    }
                    if (location != streamIdentifier.getLocationCode())
                    {
                        throw std::runtime_error("Inconsistent location code");
                    }
                }
            }
            else
            {
                msr3_free(&miniSEEDRecord);
                throw std::runtime_error("Failed to unpack SNCL");
            }
            // Sampling rate
            segment.setSamplingRate(miniSEEDRecord->samprate);
            // Start time (convert from nanoseconds to microseconds)
            // libmseed's nstime_t is already nanoseconds since the epoch.
            const std::chrono::nanoseconds startTime
            {
                static_cast<int64_t> (miniSEEDRecord->starttime)
            };
            segment.setStartTime(startTime);
            // Data
            auto nSamples = static_cast<int> (miniSEEDRecord->numsamples);
            if (nSamples > 0)
            {
                std::vector<double> data;
                if (miniSEEDRecord->sampletype == 'i')
                {
                    const auto dataPtr
                        = reinterpret_cast<const int *>
                          (miniSEEDRecord->datasamples);
                    data.resize(nSamples);
                    std::copy(dataPtr, dataPtr + data.size(), data.begin());
                }
                else if (miniSEEDRecord->sampletype == 'f')
                {
                    const auto dataPtr
                        = reinterpret_cast<const float *>
                          (miniSEEDRecord->datasamples);
                    data.resize(nSamples);
                    std::copy(dataPtr, dataPtr + data.size(), data.begin());
                }
                else if (miniSEEDRecord->sampletype == 'd')
                {
                    const auto dataPtr
                        = reinterpret_cast<const double *>
                          (miniSEEDRecord->datasamples);
                    data.resize(nSamples);
                    std::copy(dataPtr, dataPtr + data.size(), data.begin());
                }
                else
                {
                    throw std::runtime_error("Unhandled data format");
                }
                segment.setData(std::move(data));
                // No use for empty data packets
                segments.push_back(std::move(segment));
            }
            offset = offset + miniSEEDRecord->reclen;
            msr3_free(&miniSEEDRecord);
        }
        else
        {
            // Nothing advanced the offset, so continuing would spin here
            // forever on a corrupt buffer.  Whatever was parsed before
            // this point is still good and is returned.
            if (miniSEEDRecord){msr3_free(&miniSEEDRecord);}
            break;
        }
    }
    // Ensure sorted
    std::sort(segments.begin(), segments.end(),
              [](const auto &lhs, const auto &rhs)
              {
                  return lhs.getStartTime() < rhs.getStartTime();
              });
    // Purge trailing zeros
    if (purgeTrailingZeros)
    {
        // TODO way to iterate backwards?
        std::reverse(segments.begin(), segments.end());
        for (auto &segment : segments)
        {
            bool allZero{true};
            const auto &data = segment.getDataReference();
            auto nSamples = static_cast<int> (data.size());
            // Check if this is a problematic packet
            int endIndex{nSamples};
            for (int j = nSamples - 1; j >= 0; --j)
            {
                if (data[j] == 0)
                {
                    endIndex = j; 
                }
                else
                {
                    break;
                }
            }
            if (endIndex != nSamples)
            {
                if (endIndex > 0)
                {
                    auto data = segment.getData();
                    data.resize(endIndex);
                    segment.setData(std::move(data)); 
                }
                else
                {
                    // Segment is entirely zero
                    // TODO need to switch loop to iterators
                    // segments.erase(std::erase(current segment));
                }
            }
        }
        std::reverse(segments.begin(), segments.end());
    }
    if (!streamIdentifier.hasNetwork())
    {
        throw std::runtime_error("Could not determine network");
    }
    if (segments.empty())
    {
        throw std::runtime_error("No data segments");
    }
    Waveform waveform;
    waveform.setStreamIdentifier(streamIdentifier);
    // Merged: miniSEED splits a continuous record across packets, and a
    // packet boundary is not a gap in the data.
    constexpr bool merge{true};
    waveform.setSegments(std::move(segments), merge);
    return waveform;
}

/*
schema boils down to a function:

returns bytea
wave.get_waveform_blob(p_evid  event.evid%TYPE,
                       p_net waveform.net%TYPE,
                       p_sta waveform.sta%TYPE,
                       p_chan waveform.seedchan%TYPE,
                       p_loc waveform.location%TYPE,
                       p_startTime DOUBLE PRECISION, // nullable
                       p_endTime DOUBLE PRECISION); / nullable

 */

/// @note get_waveform_blob, and the time bounds are bound parameters.  The
///       earlier text named get_waveform_glob and inlined the bounds while
///       still binding $6 and $7, so the placeholders and the arguments
///       disagreed about how many there were.
constexpr std::string_view WAVEFORM_QUERY
{
R"""(
SELECT encode(wave.get_waveform_blob($1, $2, $3, $4, $5, $6, $7), 'hex');
)"""
};

}

Waveform
AQMSDutyReviewBackend::Database::AQMS::queryWaveform(
    const DB::Client &client,
    const int64_t eventIdentifier,
    const StreamIdentifier &streamIdentifier)
{
    /// These will throw (but will be upper case with no blanks unless
    /// it's the location code)
    const auto network = streamIdentifier.getNetwork();
    const auto station = streamIdentifier.getStation();
    const auto channel = streamIdentifier.getChannel();
    const auto locationCode = streamIdentifier.getLocationCode();
    /// Query - goal is to keep the query fast so we can release that
    /// transaction
    constexpr double startTime{0};        // Give me the entire waveform
    constexpr double endTime{4070908800}; // Give me the entire waveform
    const pqxx::params queryParameters{eventIdentifier,
                                       network, station, channel,
                                       locationCode,
                                       startTime, endTime};
    std::string hexadecimalData;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            const auto queryResult
                = transaction.exec(::WAVEFORM_QUERY, queryParameters);
            // Assigned, not appended: the client re-runs this after
            // re-dialling a dropped connection.
            std::string blob;
            if (!queryResult.empty())
            {
                const auto row = queryResult.one_row();
                if (!row.at(0).is_null())
                {
                    blob = row.at(0).as<std::string> ();
                }
            }
            transaction.commit();
            hexadecimalData = std::move(blob);
        },
        ::WAVEFORM_QUERY);
    // Unpack it
    if (!hexadecimalData.empty())
    {
        // encode(..., 'hex') gives two characters per byte, so the miniSEED
        // is half the length of what came back - it does not have to be
        // counted separately.
        auto miniSEED = ::hexToASCII(hexadecimalData);
        constexpr int8_t verbose{0};
        constexpr bool purgeTrailingZeros{true};
        return ::unpack(miniSEED, miniSEED.size(), verbose,
                        purgeTrailingZeros);
    }
    auto stationName = network;
    stationName.append(".").append(station).append(".").append(channel);
    if (!locationCode.empty() && locationCode != "  ")
    {
        stationName.append(".").append(locationCode);
    }
    throw std::runtime_error("No data for " + stationName);
}

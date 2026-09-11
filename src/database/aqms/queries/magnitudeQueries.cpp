#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/magnitudeQueries.hpp"
#include "durationMagnitudeReader.hpp"
#include "aqmsDutyReviewBackend/database/aqms/durationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/localMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationDurationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/stationLocalMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{

/*

 CREATE TABLE NETMAG 
 (	MAGID BIGINT, 
	ORID BIGINT NOT NULL , 
	COMMID BIGINT, 
	MAGNITUDE DOUBLE PRECISION NOT NULL , 
	MAGTYPE VARCHAR(6) NOT NULL , 
	AUTH VARCHAR(15) NOT NULL , 
	SUBSOURCE VARCHAR(8), 
	MAGALGO VARCHAR(15), 
	NSTA INTEGER, 
	UNCERTAINTY DOUBLE PRECISION, 
	GAP DOUBLE PRECISION, 
	DISTANCE DOUBLE PRECISION, 
	QUALITY DOUBLE PRECISION, 
	RFLAG VARCHAR(2), 
	LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'), 
	NOBS INTEGER,
	 CONSTRAINT NETMAG01 CHECK (magnitude >= -10.0 and magnitude <= 10.0) , 
	 CONSTRAINT NETMAG02 CHECK (magtype in ('p','a','b','e','l',
 	'l1','l2','l3','lg','c','s','w','z','B','un','d','h','n','dl','lr')) , 
	 CONSTRAINT NETMAG03 CHECK (nsta >= 0) , 
	 CONSTRAINT NETMAG04 CHECK (uncertainty >= 0.0) , 
	 CONSTRAINT NETMAG05 CHECK (quality >= 0.0 and quality <=1.0) , 
	 CONSTRAINT NETMAG06 CHECK (magid > 0) , 
	 CONSTRAINT NETMAG07 CHECK (rflag in ('a','h','f','A','H','F')) , 
	 CONSTRAINT MAGKEY01 PRIMARY KEY (MAGID) 
 ); 

 CREATE TABLE ASSOCCOM 
 (	MAGID BIGINT, 
	COID BIGINT, 
	COMMID BIGINT, 
	AUTH VARCHAR(15) NOT NULL , 
	SUBSOURCE VARCHAR(8), 
	WEIGHT DOUBLE PRECISION, 
	IN_WGT DOUBLE PRECISION, 
	RFLAG VARCHAR(2), 
	MAG DOUBLE PRECISION, 
	MAGRES DOUBLE PRECISION, 
	MAGCORR DOUBLE PRECISION, 
	LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'), 
	 CONSTRAINT ASSOCCOMKEY04 CHECK (weight >= 0.0 and weight <= 1.0) , 
	 CONSTRAINT ASSOCCOMKEY05 CHECK (in_wgt >= 0.0 and in_wgt <= 1.0) , 
	 CONSTRAINT ASSOCCOMKEY06 CHECK (rflag in ('a','h','f','A','H','F')) , 
	 CONSTRAINT ASSOCCOMKEY01 PRIMARY KEY (MAGID, COID) 
 );

 CREATE TABLE ASSOCCOO 
 (	ORID BIGINT, 
	COID BIGINT, 
	COMMID BIGINT, 
	AUTH VARCHAR(15) NOT NULL , 
	SUBSOURCE VARCHAR(8), 
	RFLAG VARCHAR(2), 
	DELTA DOUBLE PRECISION, 
	SEAZ DOUBLE PRECISION, 
	LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
       ), 
	 CONSTRAINT ASSOCCOOKEY04 CHECK (rflag in ('a','h','f','A','H','F')) , 
	 CONSTRAINT ASSOCCOOKEY01 PRIMARY KEY (ORID, COID) 
 ); 

 CREATE TABLE CODA 
 (	COID BIGINT, 
	COMMID BIGINT, 
	STA VARCHAR(6) NOT NULL , 
	NET VARCHAR(8), 
	AUTH VARCHAR(15) NOT NULL , 
	SUBSOURCE VARCHAR(8), 
	CHANNEL VARCHAR(8), 
	CHANNELSRC VARCHAR(8), 
	SEEDCHAN VARCHAR(3), 
	LOCATION VARCHAR(2), 
	CODATYPE VARCHAR(3), 
	AFIX DOUBLE PRECISION, 
	AFREE DOUBLE PRECISION, 
	QFIX DOUBLE PRECISION, 
	QFREE DOUBLE PRECISION, 
	TAU DOUBLE PRECISION, 
	NSAMPLE INTEGER, 
	RMS DOUBLE PRECISION, 
	DURTYPE VARCHAR(3), 
	IPHASE VARCHAR(8), 
	ERAMP DOUBLE PRECISION, 
	UNITS VARCHAR(4) NOT NULL , 
	TIME1 DOUBLE PRECISION, 
	AMP1 DOUBLE PRECISION, 
	TIME2 DOUBLE PRECISION, 
	AMP2 DOUBLE PRECISION, 
	TIME3 DOUBLE PRECISION, 
	AMP3 DOUBLE PRECISION, 
	TIME4 DOUBLE PRECISION, 
	AMP4 DOUBLE PRECISION, 
	TIME5 DOUBLE PRECISION, 
	AMP5 DOUBLE PRECISION, 
	TIME6 DOUBLE PRECISION, 
	AMP6 DOUBLE PRECISION, 
	QUALITY DOUBLE PRECISION, 
	RFLAG VARCHAR(2), 
	DATETIME DOUBLE PRECISION,
	ALGORITHM VARCHAR(15), 
	WINSIZE DOUBLE PRECISION, 
	LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
       ), 
	 CONSTRAINT CODA16 CHECK (time4 > 0) , 
	 CONSTRAINT CODA01 CHECK (afix >= 0.0) , 
	 CONSTRAINT CODA03 CHECK (amp1 > 0) , 
	 CONSTRAINT CODA04 CHECK (amp2 > 0) , 
	 CONSTRAINT CODA05 CHECK (amp3 > 0) , 
	 CONSTRAINT CODA06 CHECK (amp4 > 0) , 
	 CONSTRAINT CODA07 CHECK (amp5 > 0) , 
	 CONSTRAINT CODA08 CHECK (amp6 > 0) , 
	 CONSTRAINT CODA09 CHECK (codatype in ('P','S')) , 
	 CONSTRAINT CODA10 CHECK (coid > 0) , 
	 CONSTRAINT CODA11 CHECK (nsample >= 0) , 
	 CONSTRAINT CODA12 CHECK (rms >= 0.0) , 
	 CONSTRAINT CODA13 CHECK (time1 > 0) , 
	 CONSTRAINT CODA14 CHECK (time2 > 0) , 
	 CONSTRAINT CODA15 CHECK (time3 > 0) , 
	 CONSTRAINT CODA17 CHECK (time5 > 0) , 
	 CONSTRAINT CODA18 CHECK (time6 > 0) , 
	 CONSTRAINT CODA19 CHECK (rflag in ('a','h','f','A','H','F')) , 
	 CONSTRAINT CODA20 CHECK (quality >=0.0 and quality <=1.0) , 
	 CONSTRAINT CODAKEY01 PRIMARY KEY (COID) 
 ); 

*/

constexpr std::string_view ORIGIN_MAGNITUDE_QUERY
{
R"""(
SELECT netmag.magid as magnitude_identifier,
       netmag.orid as origin_identifier,
       netmag.magnitude as magnitude,
       netmag.magtype as magnitude_type
FROM netmag WHERE netmag.orid = $1
 ORDER BY netmag.magid;
)"""
};

constexpr std::string_view DURATION_MAGNITUDE_EXISTS
{
R"""(
SELECT COUNT(*) FROM netmag where netmag.orid = $1 AND magtype = 'd';
)"""
};

constexpr std::string_view LOCAL_MAGNITUDE_EXISTS
{
R"""(
SELECT COUNT(*) FROM netmag where netmag.orid = $1 AND magtype = 'l';
)"""
};

constexpr std::string_view DURATION_MAGNITUDE_QUERY
{
R"""(
SELECT netmag.magid,
       netmag.magnitude as magnitude,
       netmag.magtype as magtype,
       coda.coid as identifier,
       coda.net as network,
       coda.sta as station,
       coda.seedchan as channel,
       coda.location as location_code,
       TrueTime.getEpoch(coda.datetime, 'NOMINAL') as start_time,
       coda.tau as duration,
       coda.rflag as observation_review_flag,
       assoccom.mag as station_magnitude,
       assoccom.weight as weight,
       assoccom.magcorr as correction,
       assoccoo.orid as origin_identifier,
       assoccoo.delta as source_receiver_distance,
       assoccoo.seaz as source_receiver_azimuth
FROM netmag
LEFT OUTER JOIN assoccom
  ON assoccom.magid = netmag.magid
    LEFT OUTER JOIN coda
      ON assoccom.coid = coda.coid 
      INNER JOIN assoccoo
        ON assoccoo.coid = coda.coid
WHERE magtype = 'd' AND (assoccom.weight IS NULL OR assoccom.weight > 0) AND assoccom.magid = $1 AND assoccoo.orid = $2;
)"""
};

/// auto's aren't populating the assocamo table so this conks out
/// The per-channel static magnitude correction.
///
/// Keyed on the full channel - net, sta, seedchan, location - because that
/// is what the correction is FOR: two components at one station carry
/// different numbers.
///
/// corr_type picks the scale ('ml' or 'me'), and the row is only good for
/// the epoch it names.  ondate and offdate are TIMESTAMP WITHOUT TIME ZONE
/// holding UTC, the same convention lddate uses, so the comparison goes
/// through 'to_timestamp($n) AT TIME ZONE UTC' rather than through the
/// server's zone.
///
/// @note auth matters.  This table carries UU and UW corrections for the
///       same scale - roughly 300 and 346 'ml' rows - so a query that did
///       not name one could pick up somebody else's number.
constexpr std::string_view STATIC_CHANNEL_CORRECTION_QUERY
{
R"""(
SELECT corr
FROM stacorrections
WHERE net = $1 AND sta = $2 AND seedchan = $3 AND location = $4
  AND corr_type = $5 AND auth = $6
  AND (to_timestamp($7) AT TIME ZONE 'UTC') BETWEEN ondate AND offdate
 ORDER BY ondate DESC
 LIMIT 1;
)"""
};

/// The per-CHANNEL amplitudes behind a local magnitude.
///
/// One row per channel, deliberately.  A local magnitude is measured on
/// two horizontals - sometimes four channels - and assocamm.mag is stored
/// per component, not per station.  On an automatic magnitude those
/// components disagree badly: they match on 2.8% of stations, with spreads
/// up to 3.6 magnitude units.  There is no station value in AQMS to read,
/// so this returns what AQMS has rather than inventing a combination.
///
/// assocamo carries the geometry, keyed on (orid, ampid) - the same
/// amplitude under another origin has a different distance - which is why
/// the join needs the origin as well as the magnitude.
constexpr std::string_view LOCAL_MAGNITUDE_QUERY
{
R"""(
SELECT netmag.magid as magnitude_identifier,
       netmag.magnitude as magnitude,
       amp.ampid as identifier,
       amp.net as network,
       amp.sta as station,
       amp.seedchan as channel,
       amp.location as location_code,
       amp.amplitude as amplitude,
       amp.units as amplitude_units,
       amp.amptype as amplitude_type,
       amp.rflag as observation_review_flag,
       assocamm.mag as station_magnitude,
       assocamm.weight as weight,
       assocamm.magcorr as correction,
       assocamo.delta as source_receiver_distance,
       assocamo.seaz as source_receiver_azimuth
FROM netmag
LEFT OUTER JOIN assocamm
  ON assocamm.magid = netmag.magid
  AND (assocamm.weight IS NULL OR assocamm.weight > 0)
    LEFT OUTER JOIN amp
      ON assocamm.ampid = amp.ampid
    LEFT OUTER JOIN assocamo
      ON assocamo.ampid = amp.ampid AND assocamo.orid = $2
WHERE netmag.magtype = 'l' AND netmag.magid = $1
 ORDER BY amp.sta, amp.seedchan;
)"""
};

}

namespace
{

/// @brief Maps coda.rflag or amp.rflag onto an observation's review
///        status.
/// @note Folded to lower case for the same reason origin.rflag is - the
///       CHECK permits either.
/// @note This is the OBSERVATION's status, not the magnitude's, and the
///       two genuinely differ - the archive holds automatic codas under
///       reviewed magnitudes and finalized amplitudes under automatic
///       ones.
/// @note coda.rflag and assoccom.rflag were never observed to disagree, nor do
///       amp.rflag and assocamm.rflag, so which of the pair is read does
///       not matter.  The measurement's own is read because that is what
///       the status describes.
template<typename T>
[[nodiscard]] T toObservationReviewStatus(const std::string &rflag)
{
    std::string flag{rflag};
    std::transform(flag.begin(), flag.end(), flag.begin(), ::tolower);
    if (flag == "a"){return T::Automatic;}
    if (flag == "h"){return T::Human;}
    if (flag == "f"){return T::Finalized;}
    throw std::runtime_error("Unhandled observation review flag " + rflag);
}

/// @brief Turns one row of DURATION_MAGNITUDE_QUERY into a station
///        duration magnitude.
///
/// One row is one coda measurement on one channel.  Almost everything on
/// it is nullable and, in practice, frequently null - weight, residual,
/// correction, delta and seaz are all routinely absent - so every field
/// below the stream is checked before it is read.
[[nodiscard]] StationDurationMagnitude readStationDurationMagnitude(
    const pqxx::row_ref &row)
{
    StationDurationMagnitude stationMagnitude;

    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork(row.at("network").as<std::string> ());
    streamIdentifier.setStation(row.at("station").as<std::string> ());
    streamIdentifier.setChannel(row.at("channel").as<std::string> ());
    // A blank location code is a value in SEED, not an absence, so an
    // empty string is passed through rather than skipped.
    streamIdentifier.setLocationCode(
        row.at("location_code").is_null()
            ? std::string {}
            : row.at("location_code").as<std::string> ());
    stationMagnitude.setStreamIdentifier(std::move(streamIdentifier));

    if (!row.at("start_time").is_null())
    {
        const auto startTimeNanoSeconds
            = static_cast<std::int64_t>
              (std::round(row.at("start_time").as<double> ()*1.e9));
        stationMagnitude.setStartTime(
            std::chrono::nanoseconds {startTimeNanoSeconds});
    }
    if (!row.at("duration").is_null())
    {
        // coda.tau, in seconds.  The model refuses a non-positive
        // duration - a coda that lasted no time was not measured - so it
        // is left unset rather than forced through.
        const auto duration = row.at("duration").as<double> ();
        if (duration > 0){stationMagnitude.setDuration(duration);}
    }
    if (!row.at("observation_review_flag").is_null())
    {
        const auto reviewFlag
            = row.at("observation_review_flag").as<std::string> ();
        try
        {
            stationMagnitude.setReviewStatus(
                ::toObservationReviewStatus
                <decltype(stationMagnitude)::ReviewStatus> (reviewFlag));
        }
        catch (const std::exception &)
        {
            // An unmodelled flag leaves the status unset rather than
            // failing the observation - the magnitude is still readable.
        }
    }
    if (!row.at("station_magnitude").is_null())
    {
        stationMagnitude.setMagnitude(
            row.at("station_magnitude").as<double> ());
    }
    if (!row.at("weight").is_null())
    {
        stationMagnitude.setWeight(row.at("weight").as<double> ());
    }
    // Always computed, never read.  AQMS stores magres only when somebody
    // reviews a magnitude - zero of 140,879 automatic coda rows and zero of
    // 133,792 automatic amplitude rows carry one - so reading it would give
    // a residual on 4% of what a duty analyst opens and nothing on the rest.
    //
    // Computing it is not a substitute for the stored value; it IS the
    // stored value.  On every reviewed row measured - all of them -
    // magres equals the station magnitude less the network magnitude
    // exactly.  Doing the subtraction unconditionally means one code path
    // and one definition rather than two that agree by inspection.
    //
    // observed - estimated: the station's own magnitude is the
    // observation, the network magnitude is the estimate.
    if (stationMagnitude.hasMagnitude() && !row.at("magnitude").is_null())
    {
        stationMagnitude.setResidual(
            stationMagnitude.getMagnitude()
          - row.at("magnitude").as<double> ());
    }
    if (!row.at("correction").is_null())
    {
        stationMagnitude.setCorrection(row.at("correction").as<double> ());
    }
    if (!row.at("source_receiver_distance").is_null())
    {
        // assoccoo.delta is kilometres and the models hold meters, the
        // same conversion assocaro.delta and origin.depth get.
        stationMagnitude.setSourceReceiverDistance(
            row.at("source_receiver_distance").as<double> ()*1.e3);
    }
    if (!row.at("source_receiver_azimuth").is_null())
    {
        stationMagnitude.setSourceReceiverAzimuth(
            row.at("source_receiver_azimuth").as<double> ());
    }
    return stationMagnitude;
}


/// @brief Turns one row of LOCAL_MAGNITUDE_QUERY into a station local
///        magnitude.
///
/// PER CHANNEL.  Nothing here combines the horizontals - see the query's
/// own note for why there is nothing to combine them into.
[[nodiscard]] StationLocalMagnitude readStationLocalMagnitude(
    const pqxx::row_ref &row)
{
    StationLocalMagnitude stationMagnitude;

    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork(row.at("network").as<std::string> ());
    streamIdentifier.setStation(row.at("station").as<std::string> ());
    streamIdentifier.setChannel(row.at("channel").as<std::string> ());
    streamIdentifier.setLocationCode(
        row.at("location_code").is_null()
            ? std::string {}
            : row.at("location_code").as<std::string> ());
    stationMagnitude.setStreamIdentifier(std::move(streamIdentifier));

    if (!row.at("amplitude").is_null())
    {
        const auto amplitude = row.at("amplitude").as<double> ();
        // amp.units is not always 'mm' - the archive holds Wood-Anderson
        // amplitudes in both - and the scale is defined in millimetres, so
        // the conversion happens here rather than at every reader.
        const auto units
            = row.at("amplitude_units").is_null()
            ? std::string {}
            : row.at("amplitude_units").as<std::string> ();
        const auto inMillimeters
            = (units == "cm") ? amplitude*10.0 : amplitude;
        if (inMillimeters > 0)
        {
            stationMagnitude.setAmplitude(inMillimeters);
        }
    }
    if (!row.at("observation_review_flag").is_null())
    {
        const auto reviewFlag
            = row.at("observation_review_flag").as<std::string> ();
        try
        {
            stationMagnitude.setReviewStatus(
                ::toObservationReviewStatus
                <decltype(stationMagnitude)::ReviewStatus> (reviewFlag));
        }
        catch (const std::exception &)
        {
            // An unmodelled flag leaves the status unset rather than
            // failing the observation - the magnitude is still readable.
        }
    }
    if (!row.at("station_magnitude").is_null())
    {
        stationMagnitude.setMagnitude(
            row.at("station_magnitude").as<double> ());
    }
    if (!row.at("weight").is_null())
    {
        stationMagnitude.setWeight(row.at("weight").as<double> ());
    }
    // Always computed, never read.  AQMS stores magres only when somebody
    // reviews a magnitude - zero of 140,879 automatic coda rows and zero of
    // 133,792 automatic amplitude rows carry one - so reading it would give
    // a residual on 4% of what a duty analyst opens and nothing on the rest.
    //
    // Computing it is not a substitute for the stored value; it IS the
    // stored value.  On every reviewed row measured - all of them -
    // magres equals the station magnitude less the network magnitude
    // exactly.  Doing the subtraction unconditionally means one code path
    // and one definition rather than two that agree by inspection.
    //
    // observed - estimated: the station's own magnitude is the
    // observation, the network magnitude is the estimate.
    if (stationMagnitude.hasMagnitude() && !row.at("magnitude").is_null())
    {
        stationMagnitude.setResidual(
            stationMagnitude.getMagnitude()
          - row.at("magnitude").as<double> ());
    }
    if (!row.at("correction").is_null())
    {
        stationMagnitude.setCorrection(row.at("correction").as<double> ());
    }
    if (!row.at("source_receiver_distance").is_null())
    {
        // assocamo.delta is kilometres; the models hold meters.
        stationMagnitude.setSourceReceiverDistance(
            row.at("source_receiver_distance").as<double> ()*1.e3);
    }
    if (!row.at("source_receiver_azimuth").is_null())
    {
        stationMagnitude.setSourceReceiverAzimuth(
            row.at("source_receiver_azimuth").as<double> ());
    }
    return stationMagnitude;
}

}

std::optional<DurationMagnitude>
AQMSDutyReviewBackend::Database::AQMS::queryDurationMagnitude(
    const DB::Client &client,
    const std::int64_t originIdentifier,
    spdlog::logger *logger)
{
    std::optional<DurationMagnitude> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            const pqxx::params originParameter{originIdentifier};

            // Cheap gate first: a count on netmag alone, so an origin with
            // no duration magnitude never pays for the join out to coda.
            const auto nDurationMagnitudes
                = transaction.query_value<int>
                  (std::string {::DURATION_MAGNITUDE_EXISTS},
                   originParameter);
            if (nDurationMagnitudes < 1)
            {
                transaction.commit();
                result = std::nullopt;
                return;
            }

            // Which magnitude it is.  The existence check says one is
            // there; it does not say its magid, and the station rows are
            // keyed on that.
            std::optional<std::int64_t> magnitudeIdentifier;
            double magnitudeValue{0};
            for (const auto &row
                 : transaction.exec(std::string {::ORIGIN_MAGNITUDE_QUERY},
                                    originParameter))
            {
                if (row.at("magnitude_type").as<std::string> () != "d")
                {
                    continue;
                }
                if (magnitudeIdentifier)
                {
                    // Ordered by magid, so this keeps the lowest and says
                    // so rather than silently preferring one.
                    SPDLOG_LOGGER_WARN(
                        logger,
                        "Origin {} has more than one duration magnitude - "
                        "keeping {}",
                        originIdentifier, *magnitudeIdentifier);
                    break;
                }
                magnitudeIdentifier
                    = row.at("magnitude_identifier").as<std::int64_t> ();
                magnitudeValue = row.at("magnitude").as<double> ();
            }
            if (!magnitudeIdentifier)
            {
                // The count found one and the listing did not.  Only a
                // concurrent write explains that, and the honest answer is
                // that there is nothing to return.
                transaction.commit();
                result = std::nullopt;
                return;
            }

            DurationMagnitude magnitude;
            magnitude.setIdentifier(*magnitudeIdentifier);
            magnitude.setValue(magnitudeValue);

            auto stationMagnitudes
                = readStationDurationMagnitudes(transaction,
                                                *magnitudeIdentifier,
                                                originIdentifier,
                                                logger);
            transaction.commit();

            if (!stationMagnitudes.empty())
            {
                magnitude.setStationMagnitudes(std::move(stationMagnitudes));
            }
            result = std::move(magnitude);
        },
        ::DURATION_MAGNITUDE_QUERY);
    return result;
}

std::vector<StationDurationMagnitude>
AQMSDutyReviewBackend::Database::AQMS::readStationDurationMagnitudes(
    pqxx::transaction_base &transaction,
    const std::int64_t magnitudeIdentifier,
    const std::int64_t originIdentifier,
    spdlog::logger *logger)
{
    std::vector<StationDurationMagnitude> result;
    for (const auto &row
         : transaction.exec(std::string {::DURATION_MAGNITUDE_QUERY},
                            pqxx::params{magnitudeIdentifier,
                                         originIdentifier}))
    {
        try
        {
            result.push_back(::readStationDurationMagnitude(row));
        }
        catch (const std::exception &e)
        {
            // One unreadable coda is not worth the magnitude it belongs
            // to - the network value stands on its own.
            if (logger != nullptr)
            {
                SPDLOG_LOGGER_WARN(logger,
                                   "Skipping a coda on magnitude {} because "
                                   "{}",
                                   magnitudeIdentifier,
                                   std::string {e.what()});
            }
        }
    }
    return result;
}

std::vector<StationLocalMagnitude>
AQMSDutyReviewBackend::Database::AQMS::readStationLocalMagnitudes(
    pqxx::transaction_base &transaction,
    const std::int64_t magnitudeIdentifier,
    const std::int64_t originIdentifier,
    spdlog::logger *logger)
{
    std::vector<StationLocalMagnitude> result;
    for (const auto &row
         : transaction.exec(std::string {::LOCAL_MAGNITUDE_QUERY},
                            pqxx::params{magnitudeIdentifier,
                                         originIdentifier}))
    {
        // The outer join gives a magnitude with no amplitudes a single row
        // of nulls; there is no channel to report.
        if (row.at("identifier").is_null()){continue;}
        try
        {
            result.push_back(::readStationLocalMagnitude(row));
        }
        catch (const std::exception &e)
        {
            if (logger != nullptr)
            {
                SPDLOG_LOGGER_WARN(logger,
                                   "Skipping an amplitude on magnitude {} "
                                   "because {}",
                                   magnitudeIdentifier,
                                   std::string {e.what()});
            }
        }
    }
    return result;
}

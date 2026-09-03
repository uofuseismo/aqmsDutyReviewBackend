#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/eventQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/subnetTrigger.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{

/*
 CREATE TABLE EVENT 
 (	EVID BIGINT, 
	PREFOR BIGINT, 
	PREFMAG BIGINT, 
	PREFMEC BIGINT, 
	COMMID BIGINT, 
	AUTH VARCHAR(15) NOT NULL , 
	SUBSOURCE VARCHAR(8), 
	ETYPE VARCHAR(2) NOT NULL , 
	SELECTFLAG SMALLINT, 
	VERSION BIGINT DEFAULT (0) NOT NULL , 
	LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
       ), 
	CONSTRAINT EVENT02 CHECK (evid > 0) , 
	 CONSTRAINT EVKEY01 PRIMARY KEY (EVID),
	CONSTRAINT EVKEY02 FOREIGN KEY (ETYPE) references EventType(etype) 
 ); 

 CREATE TABLE ORIGIN 
 (	ORID BIGINT, 
	EVID BIGINT NOT NULL , 
	PREFMAG BIGINT, 
	PREFMEC BIGINT, 
	COMMID BIGINT, 
	BOGUSFLAG SMALLINT DEFAULT (0) NOT NULL , 
	DATETIME DOUBLE PRECISION NOT NULL , 
	LAT DOUBLE PRECISION NOT NULL , 
	LON DOUBLE PRECISION NOT NULL , 
	DEPTH DOUBLE PRECISION, 
	MDEPTH DOUBLE PRECISION,
	TYPE VARCHAR(2), 
	ALGORITHM VARCHAR(15), 
	ALGO_ASSOC VARCHAR(80), 
	AUTH VARCHAR(15) NOT NULL , 
	SUBSOURCE VARCHAR(8), 
	DATUMHOR VARCHAR(8), 
	DATUMVER VARCHAR(8), 
	GAP DOUBLE PRECISION, 
	DISTANCE DOUBLE PRECISION, 
	WRMS DOUBLE PRECISION, 
	STIME DOUBLE PRECISION, 
	ERHOR DOUBLE PRECISION, 
	SDEP DOUBLE PRECISION, 
	ERLAT DOUBLE PRECISION, 
	ERLON DOUBLE PRECISION, 
	TOTALARR INTEGER, 
	TOTALAMP INTEGER, 
	NDEF INTEGER, 
	NBS SMALLINT, 
	NBFM SMALLINT, 
	LOCEVID VARCHAR(12), 
	QUALITY DOUBLE PRECISION, 
	FDEPTH VARCHAR(1), 
	FEPI VARCHAR(1), 
	FTIME VARCHAR(1), 
	VMODELID VARCHAR(2), 
	CMODELID VARCHAR(2), 
	CRUST_TYPE VARCHAR(1),
	CRUST_MODEL VARCHAR(3),
	GTYPE VARCHAR(1),
	LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'
       ), 
	RFLAG VARCHAR(2), 
	 CONSTRAINT ORIGIN02 CHECK (datumhor in ('NAD27','WGS84')) , 
	 CONSTRAINT ORIGIN03 CHECK (datumver in ('NAD27','WGS84','AVERAGE')) , 
	 CONSTRAINT ORIGIN04 CHECK (depth >= -10.0 and depth <= 1000.0) , 
	 CONSTRAINT ORIGIN05 CHECK (distance >= 0.0) , 
	 CONSTRAINT ORIGIN06 CHECK (erhor >= 0.0) , 
	 CONSTRAINT ORIGIN07 CHECK (erlat >= 0.0) , 
	 CONSTRAINT ORIGIN08 CHECK (erlon >= 0.0) , 
	 CONSTRAINT ORIGIN09 CHECK (fdepth in ('y','n')) , 
	 CONSTRAINT ORIGIN10 CHECK (fepi in ('y','n')) , 
	 CONSTRAINT ORIGIN11 CHECK (ftime in ('y','n')) , 
	 CONSTRAINT ORIGIN12 CHECK (gap >= 0.0 and gap <= 360.0) , 
	 CONSTRAINT ORIGIN15 CHECK (nbfm >= 0) , 
	 CONSTRAINT ORIGIN16 CHECK (nbs >= 0) , 
	 CONSTRAINT ORIGIN17 CHECK (ndef >= 0) , 
	 CONSTRAINT ORIGIN18 CHECK (orid > 0) , 
	 CONSTRAINT ORIGIN19 CHECK (quality >= 0.0 and quality <= 1.0) , 
	 CONSTRAINT ORIGIN20 CHECK (type in ('H','h','C','c','A','a','D','d','U','u')) , 
	 CONSTRAINT ORIGIN21 CHECK (stime >= 0.0) , 
	 CONSTRAINT ORIGIN23 CHECK (wrms >= 0.0) , 
	 CONSTRAINT ORIGIN24 CHECK (sdep >=0.0) , 
	 CONSTRAINT ORIGIN25 CHECK (totalarr >=0) , 
	 CONSTRAINT ORIGIN26 CHECK (totalamp >= 0) , 
	 CONSTRAINT ORIGIN28 CHECK (rflag in ('a','h','f','i','c','A','H','F','I','C')) , 
	 CONSTRAINT ORIGIN30 CHECK (crust_type in ('H','T','E','L','V')) ,
	 CONSTRAINT ORIGIN31 CHECK (gtype in ('l','r','t')) ,
	 CONSTRAINT ORKEY01 PRIMARY KEY (ORID) 
 ); 

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

 CREATE TABLE EVENTPREFMAG 
 (	EVID BIGINT NOT NULL , 
	MAGTYPE VARCHAR(6) NOT NULL , 
	MAGID BIGINT NOT NULL , 
	LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'), 
	 CONSTRAINT EVENTPREFMAG_CHKTYP CHECK 
(magtype in 
('p','a','b','e','l',
        'l1','l2','l3','lg','c','s','w','z','B','un','d','h','n','dl','lr')) ,
	 CONSTRAINT EVENTPREFMAG_PK PRIMARY KEY (EVID, MAGTYPE) 
 ); 

CREATE TABLE credit(
    id       BIGINT    NOT NULL,
    tname    VARCHAR(30)     NOT NULL,
    refer    VARCHAR(80)     NOT NULL,
    CONSTRAINT pk_credit PRIMARY KEY (id, tname)
)
;


*/

Event::EventType toEventType(const std::string &eventType)
{
    if (eventType == "eq")
    {
        return Event::EventType::Earthquake;
    }
    else if (eventType == "qb")
    {
        return Event::EventType::QuarryBlast;
    }
    else if (eventType == "st")
    {
        return Event::EventType::SubnetTrigger;
    }
    else if (eventType == "av")
    {
        return Event::EventType::Avalanche;
    }
    else if (eventType == "co")
    {
        return Event::EventType::Collapse;
    }
    else if (eventType == "ex")
    {
        return Event::EventType::Explosion;
    }
    else if (eventType == "mi")
    {
        return Event::EventType::MiningInduced;
    }
    else if (eventType == "nt")
    {
        return Event::EventType::NuclearTest;
    }
    else if (eventType == "so")
    {
        return Event::EventType::Sonic;
    }
    else if (eventType == "uk")
    {
        return Event::EventType::Unknown;
    }   
    throw std::invalid_argument("Unhandled event type " + eventType); 
}

Origin::GeographicType toGeographicType(const std::string &gtype)
{
    if (gtype == "l")
    {   
        return Origin::GeographicType::Local;
    }   
    else if (gtype == "r")
    {   
        return Origin::GeographicType::Regional;
    }   
    else if (gtype  == "t")
    {   
        return Origin::GeographicType::Teleseismic;
    }   
    throw std::runtime_error("Unhandled geographic type " + gtype);
}

/// @brief Maps origin.rflag onto a review status.
/// @note The column permits either case - the CHECK lists both - so this
///       folds them rather than treating 'H' as unknown.
[[nodiscard]] Origin::ReviewStatus toReviewStatus(const std::string &rflag)
{
    std::string flag{rflag};
    std::transform(flag.begin(), flag.end(), flag.begin(), ::tolower);
    if (flag == "a"){return Origin::ReviewStatus::Automatic;}
    if (flag == "h"){return Origin::ReviewStatus::Human;}
    if (flag == "f"){return Origin::ReviewStatus::Finalized;}
    if (flag == "i"){return Origin::ReviewStatus::Incomplete;}
    if (flag == "c"){return Origin::ReviewStatus::Cancelled;}
    throw std::runtime_error("Unhandled review flag " + rflag);
}

/// @brief Maps netmag.magtype onto a magnitude type.
/// @result The type, or nullopt for a magtype this application has no
///         category for.
/// @note Nullopt rather than a throw or a guess.  The CHECK constraint
///       lists twenty spellings and only four have a home here; a catalog
///       row carrying an unfamiliar one still has a magnitude worth
///       showing, and calling it the wrong type would be worse than
///       leaving the type unsaid.
[[nodiscard]] std::optional<IMagnitude::Type>
    toMagnitudeType(const std::string &magType)
{
    std::string type{magType};
    std::transform(type.begin(), type.end(), type.begin(), ::tolower);
    if (type == "l" || type == "l1" || type == "l2" || type == "l3" ||
        type == "lg" || type == "lr")
    {
        return IMagnitude::Type::Local;
    }
    if (type == "d" || type == "dl"){return IMagnitude::Type::Duration;}
    if (type == "w"){return IMagnitude::Type::Moment;}
    if (type == "h"){return IMagnitude::Type::Human;}
    return std::nullopt;
}

/// This is the high-level catalog.
///
/// Every join hangs off a PREFERRED key on event, because every table it
/// reaches holds many rows per event and the catalog wants one.
///
/// The origin join is on event.prefor = origin.orid - the PREFERRED
/// origin - and not on evid.  An event has one row in origin per location
/// attempt, so joining on evid returns a relocated event once per
/// relocation, and a catalog quietly grows duplicates of exactly the
/// events an analyst has been working hardest on.
///
/// The magnitude join is on event.prefmag = NetMag.magid for the same
/// reason, and deliberately does not go through EventPrefMag.  That table
/// is keyed (evid, magtype) - it records the preferred magnitude OF EACH
/// TYPE - so joining it on evid alone returns an event once per magnitude
/// type it has, and an event with both a local and a duration magnitude
/// arrives twice.  event.prefmag names the one magnitude the database
/// considers preferred, which is the single number a duty screen shows.
///
/// Every join is a LEFT OUTER JOIN because an event need not have a
/// preferred magnitude, and the credit table need not have a row for the
/// origin - but the WHERE clause tests origin.datetime, which no null
/// origin can satisfy, so an event without an origin is dropped.  That is
/// deliberate: an event with no origin has no time or place to show.
/// A null event.prefmag matches no magid, so an event that has no
/// magnitude still arrives - with null magnitude columns, which
/// readEventSummary leaves unset and the serializer omits.
///
/// The credit join asks who located the event.  AQMS also credits
/// magnitudes, under a different tname, but the question a duty analyst
/// is asking of this screen is who located the thing, so origin is the
/// only tname wanted here.
///
/// That tname = 'origin' rides in the ON clause rather than the WHERE.
/// credit's primary key is (id, tname), so without it an origin credited
/// under several tnames would return the event once per row; and putting
/// it in the WHERE instead would quietly turn the outer join inner and
/// drop every event that has no credit at all.
///
/// Subnet triggers are excluded.  They are an Earthworm artifact rather
/// than something that happened, so they are noise on a duty review
/// screen.  The test is safe in the WHERE because event.etype is NOT
/// NULL - on a nullable column, <> would have dropped the nulls too.
constexpr std::string_view EVENT_QUERY_IN_TIME_RANGE
{
R"""(
SELECT event.evid as event_identifier,
       event.etype as event_type,
       event.version as version,
       origin.lat as latitude,
       origin.lon as longitude,
       origin.depth as depth_km,
       TrueTime.getEpoch(origin.datetime, 'NOMINAL') as origin_time,
       origin.ndef as n_defining_phases,
       origin.wrms as rms,
       origin.gap as azimuthal_gap,
       origin.subsource as origin_source,
       origin.gtype as geographic_type,
       origin.rflag as origin_review_flag,
       netmag.magnitude as magnitude,
       netmag.magtype as magnitude_type,
       credit.refer as credit
FROM event
LEFT OUTER JOIN origin
  ON event.prefor = origin.orid
  LEFT OUTER JOIN NetMag
    ON event.prefmag = NetMag.magid
    LEFT OUTER JOIN credit
    ON event.prefor = credit.id AND credit.tname = 'origin'
WHERE origin.datetime BETWEEN TrueTime.nominal2truef($1) AND TrueTime.nominal2Truef($2)
)"""
};

/// What the catalog wants: everything that is not an Earthworm artifact.
constexpr std::string_view NOT_SUBNET_TRIGGER{"  AND event.etype <> 'st'"};

/// The subnet triggers.  Its own query rather than the catalog's, because
/// almost nothing the catalog selects means anything for a trigger: there
/// is no preferred magnitude, nobody is credited with it, and the position
/// columns hold placeholders.  Selecting them only to discard them would
/// be three joins bought for nothing.
///
/// Ordered like the catalog, evid tiebreaker included - see ORDER_BY_TIME
/// for why datetime alone is not enough.  Nothing hashes this today, but
/// triggers tie on origin time far more readily than located events do,
/// and two callers of the same table disagreeing about order is its own
/// small surprise.
constexpr std::string_view SUBNET_TRIGGER_QUERY
{
R"""(
SELECT event.evid as event_identifier,
       TrueTime.getEpoch(origin.datetime, 'NOMINAL') as origin_time,
       origin.subsource as origin_source
FROM event
INNER JOIN origin
  ON event.prefor = origin.orid
WHERE origin.datetime BETWEEN TrueTime.nominal2truef($1) AND TrueTime.nominal2Truef($2)
  AND event.etype = 'st'
 ORDER BY origin.datetime DESC, event.evid DESC;
)"""
};

/// Most recent first, and evid to break a tie.  The tiebreaker is not
/// cosmetic: the catalog body is hashed so a frontend can tell whether it
/// needs to re-download, and datetime alone is not a total order.  Postgres
/// says nothing about the relative order of rows whose sort keys tie, and
/// the sort is not stable, so a plan change - an index scan instead of a
/// sequential one, a different worker count, a sort that spills - could
/// swap two events sharing an origin time, change the hash, and make every
/// client re-download a catalog that had not changed.  evid is the event
/// primary key, so this is a total order.
constexpr std::string_view ORDER_BY_TIME
{
    "\n ORDER BY origin.datetime DESC, event.evid DESC;"
};

/// The column positions, named so a change to the SELECT above cannot
/// silently swap two of them - which is exactly what makes a gap turn up
/// as an RMS error and nobody notice.
enum Column : int
{
    EventIdentifier = 0,
    EventType = 1,
    Version = 2,
    Latitude = 3,
    Longitude = 4,
    DepthInKilometers = 5,
    OriginTime = 6,
    NumberOfDefiningPhases = 7,
    WeightedRootMeanSquaredError = 8,
    AzimuthalGap = 9,
    OriginSource = 10,
    GeographicType = 11,
    OriginReviewFlag = 12,
    Magnitude = 13,
    MagnitudeType = 14,
    Credit = 15
};

/// @brief Turns one row into an EventSummary.
/// @note Only evid, etype, version, lat, and lon are NOT NULL in the
///       schema; everything else is checked before it is read.
[[nodiscard]] EventSummary readEventSummary(
    const pqxx::row_ref &row,
    spdlog::logger *logger)
{
    EventSummary eventSummary;
    eventSummary.setIdentifier(row.at(Column::EventIdentifier)
                                  .as<std::int64_t> ());
    const auto eventType
        = row.at(Column::EventType).as<std::string> ();
    try
    {
        eventSummary.setEventType(::toEventType(eventType));
    }
    catch (const std::exception &)
    {
        // A type this application does not know is still an event worth
        // showing; it is simply of unknown type.
        SPDLOG_LOGGER_WARN(logger, "Unhandled event type {} - reading as unknown",
                           eventType);
        eventSummary.setEventType(Event::EventType::Unknown);
    }
    eventSummary.setVersion(row.at(Column::Version).as<int> ());
    eventSummary.setLatitude(row.at(Column::Latitude).as<double> ());
    eventSummary.setLongitude(row.at(Column::Longitude).as<double> ());
    if (!row.at(Column::DepthInKilometers).is_null())
    {
        // AQMS stores depth in kilometers - its CHECK constraint runs from
        // -10 to 1000 - and the model works in meters.
        eventSummary.setDepth(
            row.at(Column::DepthInKilometers).as<double> ()*1.e3);
    }
    const auto originTimeNanoSeconds
        = static_cast<int64_t>
          (std::round(row.at(Column::OriginTime).as<double> ()*1.e9));
    eventSummary.setTime(std::chrono::nanoseconds {originTimeNanoSeconds});
    if (!row.at(Column::NumberOfDefiningPhases).is_null())
    {
        const auto nDefiningPhases
            = row.at(Column::NumberOfDefiningPhases).as<int> ();
        // AQMS permits zero defining phases; the model does not, because an
        // origin located by nothing was not located.  Left unset rather
        // than rejected - the rest of the row is still good.
        if (nDefiningPhases > 0)
        {
            eventSummary.setNumberOfDefiningPhases(nDefiningPhases);
        }
    }
    if (!row.at(Column::WeightedRootMeanSquaredError).is_null())
    {
        eventSummary.setWeightedRootMeanSquaredError(
            row.at(Column::WeightedRootMeanSquaredError).as<double> ());
    }
    if (!row.at(Column::AzimuthalGap).is_null())
    {
        eventSummary.setMaximumAzimuthalGap(
            row.at(Column::AzimuthalGap).as<double> ());
    }
    if (!row.at(Column::OriginSource).is_null())
    {
        eventSummary.setOriginSource(
            row.at(Column::OriginSource).as<std::string> ());
    }
    if (!row.at(Column::GeographicType).is_null())
    {
        const auto geographicType
            = row.at(Column::GeographicType).as<std::string> ();
        try
        {
            eventSummary.setGeographicType(
                ::toGeographicType(geographicType));
        }
        catch (const std::exception &)
        {
            SPDLOG_LOGGER_WARN(logger, "Unhandled geographic type {}",
                               geographicType);
        }
    }
    if (!row.at(Column::OriginReviewFlag).is_null())
    {
        const auto reviewFlag
            = row.at(Column::OriginReviewFlag).as<std::string> ();
        try
        {
            eventSummary.setReviewStatus(::toReviewStatus(reviewFlag));
        }
        catch (const std::exception &)
        {
            SPDLOG_LOGGER_WARN(logger, "Unhandled review flag {}", reviewFlag);
        }
    }
    if (!row.at(Column::Magnitude).is_null())
    {
        eventSummary.setMagnitudeValue(row.at(Column::Magnitude).as<double> ());
    }
    if (!row.at(Column::MagnitudeType).is_null())
    {
        const auto magnitudeType
            = ::toMagnitudeType(row.at(Column::MagnitudeType)
                                   .as<std::string> ());
        if (magnitudeType != std::nullopt)
        {
            eventSummary.setMagnitudeType(*magnitudeType);
        }
    }
    if (!row.at(Column::Credit).is_null())
    {
        eventSummary.setCredit(row.at(Column::Credit).as<std::string> ());
    }
    return eventSummary;
}

}

namespace
{

/// @brief Runs the event query with the given event-type predicate.
[[nodiscard]] std::vector<EventSummary> queryEventsInTimeRange(
    const DB::Client &client,
    const std::chrono::seconds &duration,
    spdlog::logger *logger,
    const std::string_view predicate)
{
    if (duration.count() <= 0)
    {
        throw std::invalid_argument("Duration must be positive");
    }
    const std::string query{std::string {::EVENT_QUERY_IN_TIME_RANGE}
                          + std::string {predicate}
                          + std::string {::ORDER_BY_TIME}};
    // system_clock, not high_resolution_clock: the latter may be
    // steady_clock, whose epoch has no relation to the wall clock, and
    // these seconds are going into a time comparison in the database.
    const auto endTime
        = std::chrono::duration_cast<std::chrono::seconds>
          (std::chrono::system_clock::now().time_since_epoch());
    const auto startTime = endTime - duration;

    std::vector<EventSummary> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            // Gathered into a local and assigned once: the client re-runs
            // this after re-dialling a dropped connection, and appending
            // straight into the result would double every row.
            std::vector<EventSummary> rows;
            const pqxx::params parameters
            {
                static_cast<double> (startTime.count()),
                static_cast<double> (endTime.count())
            };
            for (const auto &row : transaction.exec(query, parameters))
            {
                try
                {
                    rows.push_back(::readEventSummary(row, logger));
                }
                catch (const std::exception &e)
                {
                    // One unreadable row must not cost the duty analyst
                    // every other event on their screen.
                    SPDLOG_LOGGER_WARN(logger,
                                       "Skipping a catalog row because {}",
                                       std::string {e.what()});
                }
            }
            transaction.commit();
            result = std::move(rows);
        },
        query);
    return result;
}

}

std::vector<EventSummary>
AQMSDutyReviewBackend::Database::AQMS::queryEventSummaries(
    const DB::Client &client,
    const std::chrono::seconds &duration,
    spdlog::logger *logger)
{
    return ::queryEventsInTimeRange(client, duration, logger,
                                    ::NOT_SUBNET_TRIGGER);
}

std::vector<SubnetTrigger>
AQMSDutyReviewBackend::Database::AQMS::querySubnetTriggers(
    const DB::Client &client,
    const std::chrono::seconds &duration,
    spdlog::logger *logger)
{
    if (duration.count() <= 0)
    {
        throw std::invalid_argument("Duration must be positive");
    }
    const auto endTime
        = std::chrono::duration_cast<std::chrono::seconds>
          (std::chrono::system_clock::now().time_since_epoch());
    const auto startTime = endTime - duration;

    std::vector<SubnetTrigger> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            // Gathered into a local and assigned once: the client re-runs
            // this after re-dialling a dropped connection, and appending
            // straight into the result would double every row.
            std::vector<SubnetTrigger> rows;
            const pqxx::params parameters
            {
                static_cast<double> (startTime.count()),
                static_cast<double> (endTime.count())
            };
            for (const auto &row
                 : transaction.exec(::SUBNET_TRIGGER_QUERY, parameters))
            {
                try
                {
                    SubnetTrigger trigger;
                    trigger.setEventIdentifier(row.at(0).as<std::int64_t> ());
                    const auto timeNanoSeconds
                        = static_cast<int64_t>
                          (std::round(row.at(1).as<double> ()*1.e9));
                    trigger.setTime(
                        std::chrono::nanoseconds {timeNanoSeconds});
                    if (!row.at(2).is_null())
                    {
                        trigger.setOriginSource(
                            row.at(2).as<std::string> ());
                    }
                    rows.push_back(std::move(trigger));
                }
                catch (const std::exception &e)
                {
                    SPDLOG_LOGGER_WARN(logger,
                                       "Skipping a subnet trigger row "
                                       "because {}", std::string {e.what()});
                }
            }
            transaction.commit();
            result = std::move(rows);
        },
        ::SUBNET_TRIGGER_QUERY);
    return result;
}

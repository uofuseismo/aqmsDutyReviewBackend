#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <map>
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
#include "aqmsDutyReviewBackend/database/aqms/arrival.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/magnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/localMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/durationMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/humanMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/centroidMomentTensorMagnitude.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
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

        LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
         CONSTRAINT AMP02 CHECK (amplitude >0) ,
         CONSTRAINT AMP04 CHECK (AMPTYPE IN ('C','WA','WAS','PGA','PGV','PGD','WAC','WAU','IV2','SP.3','SP1.0','SP3.0', 'ML100','ME100','EGY','HEL','WASF','M0')) ,
         CONSTRAINT AMP01 CHECK (ampid > 0) ,
         CONSTRAINT AMP03 CHECK (ampmeas in ('0','1')) ,
         CONSTRAINT AMP06 CHECK (eramp >= 0.0) ,
         CONSTRAINT AMP07 CHECK (flagamp in ('P','S','R','PP','ALL','SUR')) ,
         CONSTRAINT AMP08 CHECK (per > 0.0) ,
         CONSTRAINT AMP09 CHECK (tau > 0.0) ,
         CONSTRAINT AMP10 CHECK (units in ('c','s','mm','cm','m','ms','mss','cms','cmss','cmcms','mms','mmss','mc','nm','e','iovs','spa','none','dycm')) ,
         CONSTRAINT AMP11 CHECK (quality >=0.0 and quality <=1.0) ,
         CONSTRAINT AMP12 CHECK (rflag in ('a','h','f','A','H','F')) ,
         CONSTRAINT AMP13 CHECK (cflag in ('bn', 'os','cl','BN','OS','CL')) ,
         CONSTRAINT AMPKEY01 PRIMARY KEY (AMPID)
        );
*/

[[nodiscard]] Event::EventType toEventType(const std::string &eventType)
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

[[nodiscard]] Origin::GeographicType toGeographicType(const std::string &gtype)
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

[[nodiscard]] Arrival::Phase toPhase(const std::string &phase)
{
    if (phase == "P")
    {
        return Arrival::Phase::P;
    }
    else if (phase == "S")
    {
        return Arrival::Phase::S;
    }
    throw std::runtime_error("Unhandled phase " + phase);
}

[[nodiscard]] 
Arrival::ReviewStatus toArrivalReviewStatus(const std::string &status)
{
    if (status == "A")
    {
        return Arrival::ReviewStatus::Automatic;
    }
    else if (status == "H")
    {
        return Arrival::ReviewStatus::Human;
    }
    else if (status == "F")
    {
        return Arrival::ReviewStatus::Finalized;
    }
    throw std::runtime_error("Unhandled arrival review flag + " + status);
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

/// This will pull down an extremely detailed view of multiple origins and
/// their picks.
///
/// Every origin the event has, not just the preferred one - which is what
/// separates this from the catalog query below, where every join hangs off
/// a preferred key.
///
/// The weight test rides in the ON clause and NOT in the WHERE.  An origin
/// with no associated arrivals - a relocation whose picks have not been
/// associated yet, say - has a NULL assocaro.wgt, and 'NULL > 0' is NULL
/// rather than true, so a WHERE would drop that row and with it the whole
/// origin: the outer join would quietly become an inner one.  Measured on
/// a two-origin event, the WHERE form returns one origin and this form
/// returns both.  The same reasoning is written out at length on
/// credit.tname in the catalog query, and it is the same mistake.
///
/// wgt > 0 keeps unused picks out.  An arrival associated at zero weight
/// was considered and not used, so it did not contribute to the location;
/// showing it beside the ones that did would misrepresent the solution.
///
/// event.prefor is selected because Event::setOrigins requires exactly one
/// origin to be marked preferred and refuses the lot otherwise.  Nothing
/// on the origin row says which one that is - it is the event that names
/// its preferred origin - so without this column every origin would come
/// back not-preferred and the event could not be assembled at all.
///
/// The ORDER BY is load-bearing: readEvent groups arrivals onto origins by
/// watching origin_identifier change from one row to the next, which only
/// works while the rows for one origin are contiguous.  Reordering this
/// does not silently corrupt anything - the same origin would arrive twice
/// and setOrigins rejects a duplicate identifier - but it does turn a
/// working query into a failing one.
constexpr std::string_view EVENT_AND_ORIGIN_INFORMATION_QUERY
{
R"""(
SELECT event.evid as event_identifier,
       event.etype as event_type,
       event.version as version,
       event.prefor as preferred_origin_identifier,
       event.prefmag as preferred_magnitude_identifier,
       origin.orid as origin_identifier,
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
       arrival.arid as arrival_identifier,
       arrival.net as network,
       arrival.sta as station,
       arrival.seedchan as channel,
       arrival.location as location_code,
       TrueTime.getEpoch(arrival.datetime, 'NOMINAL') as arrival_time,
       arrival.quality as quality,
       arrival.iphase as phase,
       arrival.rflag as arrival_review_flag,
       assocaro.timeres as residual,
       assocaro.delta as source_receiver_distance,
       assocaro.seaz as source_receiver_azimuth
FROM event
LEFT OUTER JOIN origin
  ON event.evid = origin.evid
  LEFT OUTER JOIN assocaro
    ON origin.orid = assocaro.orid AND assocaro.wgt > 0
    LEFT OUTER JOIN arrival
    ON assocaro.arid = arrival.arid
WHERE event.evid = $1
 ORDER BY origin.orid, arrival.datetime;
)"""
};

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

/// @brief Maps netmag.rflag onto a magnitude review status.
/// @note Folded to lower case for the same reason origin.rflag is - the
///       column carries either.
/// @note 'F' - finalized - collapses onto Human.  IMagnitude::ReviewStatus
///       only distinguishes "a machine produced this" from "a person stood
///       behind it", and a finalized magnitude is emphatically the latter.
///       The distinction between reviewed and published lives on the
///       ORIGIN, which has a Finalized status of its own.
[[nodiscard]] IMagnitude::ReviewStatus
toMagnitudeReviewStatus(const std::string &rflag)
{
    std::string flag{rflag};
    std::transform(flag.begin(), flag.end(), flag.begin(), ::tolower);
    if (flag == "a"){return IMagnitude::ReviewStatus::Automatic;}
    if (flag == "h"){return IMagnitude::ReviewStatus::Human;}
    if (flag == "f"){return IMagnitude::ReviewStatus::Human;}
    throw std::runtime_error("Unhandled magnitude review flag " + rflag);
}

/// The magnitudes belonging to a set of origins.
///
/// A separate statement rather than another join on the detailed query
/// above.  That query already fans out one row per (origin, arrival), and
/// joining magnitudes would multiply it again by magnitudes per origin -
/// every arrival repeated once per magnitude, for data that has nothing to
/// do with arrivals.  Lifting the origin identifiers and asking a second
/// question is cheaper to run and very much easier to read.
///
/// Run inside the SAME transaction as the query above, so both see one
/// snapshot.  Across two transactions a relocation landing in between
/// could produce magnitudes for an origin the first query never returned.
///
/// origin.prefmag rides along because it is what marks an origin's
/// preferred magnitude, and it is per-origin rather than per-magnitude.
constexpr std::string_view NETMAG_QUERY
{
R"""(
SELECT netmag.magid as magnitude_identifier,
       netmag.orid as origin_identifier,
       netmag.magnitude as magnitude,
       netmag.magtype as magnitude_type,
       netmag.rflag as magnitude_review_flag,
       origin.prefmag as origin_preferred_magnitude_identifier
FROM netmag
INNER JOIN origin
  ON netmag.orid = origin.orid
WHERE netmag.orid = ANY($1::bigint[])
 ORDER BY netmag.orid, netmag.magid;
)"""
};

/// @brief Makes an empty magnitude of the given type.
/// @note The type is what decides the class, and the class is what decides
///       how much more work there is: Human and Moment are a value and
///       little else, while Local and Duration each carry the per-station
///       magnitudes that produced them - stamag rows, fetched separately
///       once those queries exist.
[[nodiscard]] std::unique_ptr<IMagnitude>
makeMagnitude(const IMagnitude::Type type)
{
    switch (type)
    {
    case IMagnitude::Type::Local:
        return std::make_unique<LocalMagnitude> ();
    case IMagnitude::Type::Duration:
        return std::make_unique<DurationMagnitude> ();
    case IMagnitude::Type::Human:
        return std::make_unique<HumanMagnitude> ();
    case IMagnitude::Type::Moment:
        return std::make_unique<CentroidMomentTensorMagnitude> ();
    }
    throw std::runtime_error("Unhandled magnitude type");
}

/// One netmag row, kept only until the winner for its type is known.
struct MagnitudeRow
{
    double value{0};
    std::int64_t identifier{0};
    IMagnitude::ReviewStatus reviewStatus{IMagnitude::ReviewStatus::Automatic};
    bool hasReviewStatus{false};
    bool isPreferred{false};
};

/// @brief Reads the netmag rows into magnitudes, keyed by origin.
///
/// Origin::setMagnitudes permits one magnitude per TYPE, and netmag does
/// not: 'l', 'l1' and 'l2' all mean local, and an origin can carry more
/// than one of them.  So a type is decided here rather than rejected
/// later - the one origin.prefmag names wins, and failing that the lowest
/// magid, which the ORDER BY makes the first one seen.  The alternative is
/// setMagnitudes throwing and the origin losing every magnitude it has,
/// including the ones there was no argument about.
[[nodiscard]] std::map<std::int64_t, std::vector<std::unique_ptr<IMagnitude>>>
readMagnitudes(const pqxx::result &rows, spdlog::logger *logger)
{
    // origin -> type -> the row that won that type.
    std::map<std::int64_t, std::map<IMagnitude::Type, MagnitudeRow>> chosen;
    for (const auto &row : rows)
    {
        const auto originIdentifier
            = row.at("origin_identifier").as<std::int64_t> ();
        const auto magnitudeType
            = row.at("magnitude_type").as<std::string> ();
        const auto type = ::toMagnitudeType(magnitudeType);
        if (type == std::nullopt)
        {
            // Energy and 'n' magnitudes among others - deliberately not
            // modelled, so skipped rather than guessed at.
            SPDLOG_LOGGER_DEBUG(logger,
                                "Skipping magnitude of unmodelled type {}",
                                magnitudeType);
            continue;
        }
        if (row.at("magnitude").is_null())
        {
            SPDLOG_LOGGER_WARN(logger,
                               "Skipping magnitude {} on origin {} - no value",
                               row.at("magnitude_identifier").as<std::int64_t> (),
                               originIdentifier);
            continue;
        }
        MagnitudeRow candidate;
        candidate.identifier
            = row.at("magnitude_identifier").as<std::int64_t> ();
        candidate.value = row.at("magnitude").as<double> ();
        if (!row.at("origin_preferred_magnitude_identifier").is_null())
        {
            candidate.isPreferred
                = row.at("origin_preferred_magnitude_identifier")
                     .as<std::int64_t> () == candidate.identifier;
        }
        if (!row.at("magnitude_review_flag").is_null())
        {
            const auto reviewFlag
                = row.at("magnitude_review_flag").as<std::string> ();
            try
            {
                candidate.reviewStatus
                    = ::toMagnitudeReviewStatus(reviewFlag);
                candidate.hasReviewStatus = true;
            }
            catch (const std::exception &)
            {
                SPDLOG_LOGGER_WARN(logger,
                                   "Unhandled magnitude review flag {}",
                                   reviewFlag);
            }
        }

        auto &byType = chosen[originIdentifier];
        const auto existing = byType.find(*type);
        if (existing == byType.end())
        {
            byType.emplace(*type, candidate);
            continue;
        }
        if (existing->second.isPreferred || !candidate.isPreferred)
        {
            // The incumbent stays: either it is the preferred one, or
            // neither is and the first seen keeps the place.
            SPDLOG_LOGGER_DEBUG(logger,
                                "Origin {} has more than one magnitude of "
                                "type {} - keeping {}",
                                originIdentifier, magnitudeType,
                                existing->second.identifier);
            continue;
        }
        existing->second = candidate;
    }

    std::map<std::int64_t, std::vector<std::unique_ptr<IMagnitude>>> result;
    for (const auto &[originIdentifier, byType] : chosen)
    {
        std::vector<std::unique_ptr<IMagnitude>> magnitudes;
        magnitudes.reserve(byType.size());
        for (const auto &[type, chosenRow] : byType)
        {
            auto magnitude = ::makeMagnitude(type);
            magnitude->setIdentifier(chosenRow.identifier);
            magnitude->setValue(chosenRow.value);
            if (chosenRow.hasReviewStatus)
            {
                magnitude->setReviewStatus(chosenRow.reviewStatus);
            }
            if (chosenRow.isPreferred)
            {
                magnitude->setIsPreferred();
            }
            else
            {
                magnitude->setNotPreferred();
            }
            // TODO The station magnitudes belonging to a Local or a
            //      Duration magnitude come from stamag and are not fetched
            //      yet.  They hang off THIS object - see
            //      LocalMagnitude::setStationMagnitudes - so the query for
            //      them keys on magid and is another statement in the same
            //      transaction, exactly as this one is.
            magnitudes.push_back(std::move(magnitude));
        }
        result.emplace(originIdentifier, std::move(magnitudes));
    }
    return result;
}

/// @brief Turns one row into an EventSummary.
/// @note Only evid, etype, version, lat, and lon are NOT NULL in the
///       schema; everything else is checked before it is read.
[[nodiscard]] EventSummary readEventSummary(
    const pqxx::row_ref &row,
    spdlog::logger *logger)
{
    /// The column positions, named so a change to the SELECT above cannot
    /// silently swap two of them - which is exactly what makes a gap turn up
    /// as an RMS error and nobody notice.
    enum EventSummaryColumn : std::int8_t
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

    EventSummary eventSummary;
    eventSummary.setIdentifier(row.at(EventSummaryColumn::EventIdentifier)
                                  .as<std::int64_t> ());
    const auto eventType
        = row.at(EventSummaryColumn::EventType).as<std::string> ();
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
    eventSummary.setVersion(row.at(EventSummaryColumn::Version).as<int> ());
    eventSummary.setLatitude(
        row.at(EventSummaryColumn::Latitude).as<double> ());
    eventSummary.setLongitude(
        row.at(EventSummaryColumn::Longitude).as<double> ());
    if (!row.at(EventSummaryColumn::DepthInKilometers).is_null())
    {
        // AQMS stores depth in kilometers - its CHECK constraint runs from
        // -10 to 1000 - and the model works in meters.
        eventSummary.setDepth(
            row.at(EventSummaryColumn::DepthInKilometers).as<double> ()*1.e3);
    }
    const auto originTimeNanoSeconds
        = static_cast<int64_t>
          (std::round(row.at(EventSummaryColumn::OriginTime).as<double> ()*1.e9));
    eventSummary.setTime(std::chrono::nanoseconds {originTimeNanoSeconds});
    if (!row.at(EventSummaryColumn::NumberOfDefiningPhases).is_null())
    {
        const auto nDefiningPhases
            = row.at(EventSummaryColumn::NumberOfDefiningPhases).as<int> ();
        // AQMS permits zero defining phases; the model does not, because an
        // origin located by nothing was not located.  Left unset rather
        // than rejected - the rest of the row is still good.
        if (nDefiningPhases > 0)
        {
            eventSummary.setNumberOfDefiningPhases(nDefiningPhases);
        }
    }
    if (!row.at(EventSummaryColumn::WeightedRootMeanSquaredError).is_null())
    {
        eventSummary.setWeightedRootMeanSquaredError(
            row.at(EventSummaryColumn::WeightedRootMeanSquaredError).as<double> ());
    }
    if (!row.at(EventSummaryColumn::AzimuthalGap).is_null())
    {
        eventSummary.setMaximumAzimuthalGap(
            row.at(EventSummaryColumn::AzimuthalGap).as<double> ());
    }
    if (!row.at(EventSummaryColumn::OriginSource).is_null())
    {
        eventSummary.setOriginSource(
            row.at(EventSummaryColumn::OriginSource).as<std::string> ());
    }
    if (!row.at(EventSummaryColumn::GeographicType).is_null())
    {
        const auto geographicType
            = row.at(EventSummaryColumn::GeographicType).as<std::string> ();
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
    if (!row.at(EventSummaryColumn::OriginReviewFlag).is_null())
    {
        const auto reviewFlag
            = row.at(EventSummaryColumn::OriginReviewFlag).as<std::string> ();
        try
        {
            eventSummary.setReviewStatus(::toReviewStatus(reviewFlag));
        }
        catch (const std::exception &)
        {
            SPDLOG_LOGGER_WARN(logger, "Unhandled review flag {}", reviewFlag);
        }
    }
    if (!row.at(EventSummaryColumn::Magnitude).is_null())
    {
        eventSummary.setMagnitudeValue(row.at(EventSummaryColumn::Magnitude).as<double> ());
    }
    if (!row.at(EventSummaryColumn::MagnitudeType).is_null())
    {
        const auto magnitudeType
            = ::toMagnitudeType(row.at(EventSummaryColumn::MagnitudeType)
                                   .as<std::string> ());
        if (magnitudeType != std::nullopt)
        {
            eventSummary.setMagnitudeType(*magnitudeType);
        }
    }
    if (!row.at(EventSummaryColumn::Credit).is_null())
    {
        eventSummary.setCredit(row.at(EventSummaryColumn::Credit).as<std::string> ());
    }
    return eventSummary;
}

/// @brief Turns one row into an Origin, WITHOUT its arrivals.
/// @note The arrivals are the caller's job.  One row is one (origin,
///       arrival) pair, so an origin's picks are spread over as many rows
///       as it has picks and no single row can fill them in.  readEvent
///       walks those rows and hands the finished set to setArrivals.
/// @note Marked preferred by comparing this origin against the event's
///       prefor, which is the only place that fact is recorded.
[[nodiscard]] Origin readOrigin(const pqxx::row_ref &row)
{
    Origin origin;
    const auto identifier = row.at("origin_identifier").as<std::int64_t> ();
    origin.setIdentifier(identifier);
    origin.setLatitude(row.at("latitude").as<double> ());
    origin.setLongitude(row.at("longitude").as<double> ());
    if (!row.at("depth_km").is_null())
    {
        // AQMS stores depth in kilometers - its CHECK runs from -10 to
        // 1000 - and the model works in meters.
        origin.setDepth(row.at("depth_km").as<double> ()*1.e3);
    }
    const auto originTimeNanoSeconds
        = static_cast<std::int64_t>
          (std::round(row.at("origin_time").as<double> ()*1.e9));
    origin.setTime(std::chrono::nanoseconds {originTimeNanoSeconds});
    if (!row.at("geographic_type").is_null())
    {
        origin.setGeographicType(
            ::toGeographicType(row.at("geographic_type").as<std::string> ()));
    }
    if (!row.at("origin_review_flag").is_null())
    {
        origin.setReviewStatus(
            ::toReviewStatus(row.at("origin_review_flag").as<std::string> ()));
    }
    // Not preferred until the event says so.  A NULL prefor leaves every
    // origin unpreferred, which readEvent notices and reports rather than
    // guessing.
    origin.setNotPreferred();
    if (!row.at("preferred_origin_identifier").is_null())
    {
        if (row.at("preferred_origin_identifier").as<std::int64_t> ()
            == identifier)
        {
            origin.setIsPreferred();
        }
    }
    return origin;
}

Arrival readArrival(const pqxx::row_ref &row)
{
    Arrival arrival;
    arrival.setIdentifier(row.at("arrival_identifier").as<int64_t> ());

    auto arrivalTime
        = static_cast<int64_t>
          (std::round(row.at("arrival_time").as<double> ()*1.e9)); 
    arrival.setTime(std::chrono::nanoseconds {arrivalTime});
    StreamIdentifier streamIdentifier;
    streamIdentifier.setNetwork(row.at("network").as<std::string> ());
    streamIdentifier.setStation(row.at("station").as<std::string> ());
    streamIdentifier.setChannel(row.at("channel").as<std::string> ());
    streamIdentifier.setLocationCode(
        row.at("location_code").as<std::string> ());
    arrival.setStreamIdentifier(std::move(streamIdentifier));

    if (!row.at("quality").is_null())
    {
        arrival.setQuality(row["quality"].as<double> ());
    }
    if (!row.at("arrival_review_flag").is_null())
    {
        arrival.setReviewStatus(
            ::toArrivalReviewStatus(
               row["arrival_review_flag"].as<std::string> ()
            )
        );
    }
    if (!row.at("phase").is_null())
    {
        arrival.setPhase(::toPhase(row.at("phase").as<std::string> ()));
    }
    // The three below come from assocaro rather than arrival: they
    // describe this pick's association with THIS origin, so the same pick
    // on another origin carries different ones.
    if (!row.at("residual").is_null())
    {
        const auto residualNanoSeconds
            = static_cast<std::int64_t>
              (std::round(row.at("residual").as<double> ()*1.e9));
        arrival.setResidual(std::chrono::nanoseconds {residualNanoSeconds});
    }
    if (!row.at("source_receiver_distance").is_null())
    {
        arrival.setSourceReceiverDistance(
            row.at("source_receiver_distance").as<double> ());
    }
    if (!row.at("source_receiver_azimuth").is_null())
    {
        arrival.setSourceReceiverAzimuth(
            row.at("source_receiver_azimuth").as<double> ());
    }
    return arrival;
}

/// @brief Assembles one event from the rows of
///        EVENT_AND_ORIGIN_INFORMATION_QUERY.
///
/// One row is one (origin, arrival) pair, so an origin with n picks
/// occupies n rows and an origin with none still occupies one - with its
/// arrival columns null, because the join is an outer one.  The origin
/// columns repeat identically down every row of a group, which is why the
/// origin is parsed once per group rather than once per row: watch
/// origin_identifier, and when it changes, the previous origin is complete.
///
/// This depends on the query's ORDER BY keeping a group's rows together.
/// If that is ever dropped the same origin arrives in two pieces, and
/// setOrigins refuses a duplicate identifier rather than quietly building
/// a wrong event.
///
/// @throws std::runtime_error if the rows carry no origin at all - an
///         event with no origin has no location, and there is nothing to
///         review.
[[nodiscard]] Event readEvent(
    const pqxx::result &rows,
    std::map<std::int64_t,
             std::vector<std::unique_ptr<IMagnitude>>> &&magnitudesByOrigin,
    spdlog::logger *logger)
{
    if (rows.empty())
    {
        throw std::runtime_error("No rows to build an event from");
    }
    Event event;
    // The event columns repeat down every row, so the first will do.
    const auto &firstRow = rows.front();
    event.setIdentifier(firstRow.at("event_identifier").as<std::int64_t> ());
    event.setVersion(firstRow.at("version").as<int> ());
    const auto eventType = firstRow.at("event_type").as<std::string> ();
    try
    {
        event.setEventType(::toEventType(eventType));
    }
    catch (const std::exception &)
    {
        // Unknown rather than fatal, the same way the catalog handles it:
        // an event type nobody has taught this about is still an event.
        SPDLOG_LOGGER_WARN(logger, "Unhandled event type {}", eventType);
        event.setEventType(Event::EventType::Unknown);
    }

    std::vector<Origin> origins;
    std::vector<Arrival> arrivals;
    Origin currentOrigin;
    std::optional<std::int64_t> currentIdentifier;
    const auto flushOrigin
        = [&]()
          {
              if (!currentIdentifier){return;}
              if (!arrivals.empty())
              {
                  currentOrigin.setArrivals(std::move(arrivals));
                  arrivals.clear();
              }
              const auto found = magnitudesByOrigin.find(*currentIdentifier);
              if (found != magnitudesByOrigin.end() && !found->second.empty())
              {
                  try
                  {
                      currentOrigin.setMagnitudes(std::move(found->second));
                  }
                  catch (const std::exception &e)
                  {
                      // setMagnitudes refuses a set with no preferred
                      // magnitude, which is what an origin whose prefmag is
                      // null or points outside its own magnitudes gives.
                      // The origin itself is still good - it has a position
                      // and a time - so it goes on without them rather than
                      // taking the whole event down with it.
                      SPDLOG_LOGGER_WARN(
                          logger,
                          "Dropping the magnitudes on origin {} because {}",
                          *currentIdentifier, std::string {e.what()});
                  }
              }
              origins.push_back(std::move(currentOrigin));
          };

    for (const auto &row : rows)
    {
        // The outer join gives an event with no origins a single row of
        // nulls; there is nothing to group.
        if (row.at("origin_identifier").is_null()){continue;}
        const auto identifier
            = row.at("origin_identifier").as<std::int64_t> ();
        if (!currentIdentifier || *currentIdentifier != identifier)
        {
            flushOrigin();
            currentOrigin = ::readOrigin(row);
            currentIdentifier = identifier;
        }
        // Null for an origin whose picks are not associated yet, and for
        // one whose every pick was associated at zero weight.
        if (row.at("arrival_identifier").is_null()){continue;}
        try
        {
            arrivals.push_back(::readArrival(row));
        }
        catch (const std::exception &e)
        {
            // One unreadable pick must not cost the analyst the origin it
            // hangs off, let alone the other origins.
            SPDLOG_LOGGER_WARN(logger,
                               "Skipping an arrival on origin {} because {}",
                               identifier, std::string {e.what()});
        }
    }
    flushOrigin();

    if (origins.empty())
    {
        throw std::runtime_error(
            "Event " + std::to_string(event.getIdentifier())
          + " has no origins");
    }
    // setOrigins insists on exactly one preferred origin and refuses the
    // whole set otherwise, so an event whose prefor is null or names an
    // origin that is not here would be unreadable.  Naming the problem
    // beats letting setOrigins report it as a count.
    const auto nPreferred
        = std::count_if(origins.begin(), origins.end(),
                        [](const Origin &origin)
                        {
                            return origin.isPreferred();
                        });
    if (nPreferred != 1)
    {
        throw std::runtime_error(
            "Event " + std::to_string(event.getIdentifier()) + " has "
          + std::to_string(nPreferred) + " preferred origins out of "
          + std::to_string(origins.size())
          + " - event.prefor is null or names an origin this query did "
            "not return");
    }
    event.setOrigins(std::move(origins));
    // After the origins, because Event resolves this against them.  Left
    // unset when AQMS has no preferred magnitude for the event, which is
    // ordinary for one nobody has sized yet.
    if (!firstRow.at("preferred_magnitude_identifier").is_null())
    {
        event.setPreferredMagnitudeIdentifier(
            firstRow.at("preferred_magnitude_identifier").as<std::int64_t> ());
        if (!event.hasPreferredMagnitude())
        {
            // Not fatal, but worth saying out loud: the event names a
            // preferred magnitude no origin here carries, so
            // preferredMagnitude() will throw for whoever asks next.
            SPDLOG_LOGGER_WARN(
                logger,
                "Event {} names preferred magnitude {} which none of its "
                "origins holds",
                event.getIdentifier(),
                event.getPreferredMagnitudeIdentifier());
        }
    }
    return event;
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

std::optional<Event>
AQMSDutyReviewBackend::Database::AQMS::queryEvent(
    const DB::Client &client,
    const std::int64_t eventIdentifier,
    spdlog::logger *logger)
{
    std::optional<Event> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            // ONE transaction for both statements.  The second is keyed on
            // origin identifiers the first returned, so run separately a
            // relocation landing in between would leave magnitudes for an
            // origin that is not in hand - or an origin whose magnitudes
            // had just been replaced.  Inside one transaction both read
            // the same snapshot and cannot disagree.
            pqxx::work transaction(connection);
            const auto originRows
                = transaction.exec(::EVENT_AND_ORIGIN_INFORMATION_QUERY,
                                   pqxx::params{eventIdentifier});
            if (originRows.empty())
            {
                // No such event.  Not an error - a client may ask about an
                // identifier that has since been merged away.
                transaction.commit();
                result = std::nullopt;
                return;
            }

            // The origin identifiers, to ask netmag about.  Taken straight
            // off the rows rather than from the parsed origins, because
            // the magnitudes have to be in hand BEFORE readEvent builds
            // them - Origin::setMagnitudes is how they get attached.
            std::vector<std::int64_t> originIdentifiers;
            for (const auto &row : originRows)
            {
                if (row.at("origin_identifier").is_null()){continue;}
                const auto identifier
                    = row.at("origin_identifier").as<std::int64_t> ();
                if (originIdentifiers.empty() ||
                    originIdentifiers.back() != identifier)
                {
                    // The query orders by orid, so equal identifiers are
                    // adjacent and this is enough to make them distinct.
                    originIdentifiers.push_back(identifier);
                }
            }

            std::map<std::int64_t,
                     std::vector<std::unique_ptr<IMagnitude>>> magnitudes;
            if (!originIdentifiers.empty())
            {
                // A bigint[] literal - "{100,200}".  The values came out
                // of the database as integers a moment ago, so there is
                // nothing here to quote, and it still travels as a bound
                // parameter rather than as text spliced into the SQL.
                std::string array{"{"};
                for (std::size_t i = 0; i < originIdentifiers.size(); ++i)
                {
                    if (i > 0){array += ",";}
                    array += std::to_string(originIdentifiers.at(i));
                }
                array += "}";
                magnitudes
                    = ::readMagnitudes(
                          transaction.exec(::NETMAG_QUERY,
                                           pqxx::params{array}),
                          logger);
            }

            auto event = ::readEvent(originRows, std::move(magnitudes),
                                     logger);
            transaction.commit();
            result = std::move(event);
        },
        ::EVENT_AND_ORIGIN_INFORMATION_QUERY);
    return result;
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

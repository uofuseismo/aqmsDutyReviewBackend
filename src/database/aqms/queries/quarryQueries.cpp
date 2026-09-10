#include <chrono>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/quarryQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/quarry.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{

constexpr std::chrono::seconds FAR_FUTURE{32472144000};  // 2999-01-01Z

/*
 CREATE TABLE GAZETTEERQUARRY
 (      gazid BIGINT NOT NULL,
        ondate TIMESTAMP NOT NULL,
        offdate TIMESTAMP DEFAULT ('3000-01-01 00:00:00'::Timestamp),
        RADIUS DOUBLE PRECISION,
        MDEPTH DOUBLE PRECISION,
        MMAG DOUBLE PRECISION,
        DAYS VARCHAR(7)[],
        STIME VARCHAR(8)[],
        ETIME VARCHAR(8)[],
        TFLG INTEGER,
        REMARK VARCHAR(80),
        LDDATE TIMESTAMP DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
         CONSTRAINT GAZQRY_PK_GAZID PRIMARY KEY (GAZID, ONDATE)
 );

CREATE TABLE gazetteerpt(
    gazid     BIGINT    NOT NULL,
    type      SMALLINT  NOT NULL,
    lat       DOUBLE PRECISION    NOT NULL,
    lon       DOUBLE PRECISION   NOT NULL,
    z         DOUBLE PRECISION,
    name      VARCHAR(48)     NOT NULL,
    state     VARCHAR(2),
    county    VARCHAR(32),
    map       VARCHAR(48),
    datumh    VARCHAR(8),
    datumv    VARCHAR(8),
    lddate    TIMESTAMP             DEFAULT (CURRENT_TIMESTAMP AT TIME ZONE 'UTC'),
    CONSTRAINT gazpt_pk_gazid PRIMARY KEY (gazid)
)
;
*/

constexpr std::string_view QUARRY_QUERY
{
R"""(
SELECT gazetteerpt.lat as latitude,
       gazetteerpt.lon as longitude,
       gazetteerpt.name as name,
       EXTRACT(EPOCH FROM gazetteerquarry.ondate)::BIGINT as ondate,
       EXTRACT(EPOCH FROM gazetteerquarry.offdate)::BIGINT as offdate,
       EXTRACT(EPOCH FROM gazetteerquarry.lddate)::BIGINT as lddate
FROM gazetteerpt 
INNER JOIN gazetteerquarry 
 ON gazetteerquarry.gazid = gazetteerpt.gazid;
ORDER BY ondate, name;
)"""
};

/// @brief Turns one row into a Quarry.
/// @note pqxx::row_ref, not pqxx::row: iterating a result yields a
///       lightweight reference into it rather than a copied row.
[[nodiscard]] Quarry readQuarry(const pqxx::row_ref &row)
{
    Quarry quarry;
    quarry.setLatitude(row.at("latitude").as<double> ());
    quarry.setLongitude(row.at("longitude").as<double> ());
    quarry.setName(row.at("name").as<std::string> ());
    const std::chrono::seconds onDate{row.at("ondate").as<long long> ()};
    const auto offDate = row.at("offdate").is_null()
                       ? ::FAR_FUTURE
                       : std::chrono::seconds{row.at("offdate").as<long long> ()};
    quarry.setStartAndEndTime({onDate, offDate});
    const std::chrono::seconds loadTime{row.at("lddate").as<long long> ()};
    quarry.setLoadTime(loadTime);
    return quarry;
}

[[nodiscard]] std::vector<Quarry> runQuery(const DB::Client &client,
                                           const std::string &query,
                                           const pqxx::params &parameters)
{
    std::vector<Quarry> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            std::vector<Quarry> rows;
            for (const auto &row : transaction.exec(query, parameters))
            {
                rows.push_back(::readQuarry(row));
            }
            transaction.commit();
            result = std::move(rows);
        },
        query);
    return result;
}

}

std::vector<Quarry>
AQMSDutyReviewBackend::Database::AQMS::queryQuarries(const DB::Client &client)
{
    const std::string query{std::string {::QUARRY_QUERY}};
    return ::runQuery(client, query, pqxx::params{});
}


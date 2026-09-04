#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/database/aqms/queries/eventLockQueries.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{
/// How much older than the catalog window a lock may be and still be
/// reported.
///
/// The filter exists to drop locks the frontend has no event to attach
/// them to, so in principle the two windows should be the same.  This is
/// slack against the ways they can fail to line up: the catalog window is
/// anchored to this process' clock and lddate is written by whatever host
/// runs AQMS, so the two disagree by whatever their clocks disagree by;
/// and /catalog and /locked-events are separate requests taken at
/// different instants.  Thirty minutes is far more than either needs and
/// still far less than the fortnight the catalog covers, so it costs
/// nothing to be generous here.
///
/// Erring towards reporting a lock is the safe direction.  A lock shown
/// for an event that is not on screen is invisible; a lock withheld for an
/// event that IS on screen reads as an unlocked event, and that is how two
/// analysts end up working the same one.
constexpr std::chrono::minutes LOCK_WINDOW_TOLERANCE{30};

/// @note The WHERE narrows to locks that could belong to an event in the
///       catalog window.  A lock cannot predate the event it holds, so a
///       lock older than that window belongs to an event the frontend is
///       not showing - typically one somebody left held and never
///       released.
///
/// @note A NULL lddate is kept, not filtered out.  It would otherwise
///       vanish silently: 'NULL > anything' is NULL, which is not true,
///       so the row would fail the predicate without ever being
///       considered old.  A lock nobody can date is a lock that cannot be
///       ruled out, and dropping it reports a held event as free.
/// lddate is when AQMS last wrote the row, which for this table is when
/// the lock was acquired.  The review tool INSERTS a row when it opens an
/// event and DELETES it when it closes one; it never updates one in
/// place, so a row's lddate is the acquisition that created it and there
/// is no earlier acquisition it could be confused with.  That is why the
/// field is acquiredAt and not lastAcquired - "last" would imply the row
/// outlives an acquisition and records the most recent one, and it does
/// not.
///
/// The exception is a review tool that crashes.  Its row is not deleted,
/// so the event stays locked until somebody removes the row by hand, and
/// until then lddate says when the crashed session took it - which is
/// exactly what whoever is clearing it wants to know.
///
/// Formatted in SQL, to the same RFC 3339 UTC shape list_users emits -
/// "2026-09-04T20:43:26Z" - so the frontend parses one format of
/// timestamp and not two.
///
/// @warning The incantation is NOT the one list_users uses, and copying
///          that one here silently shifts every lock by the server's
///          offset.  users.created is a TIMESTAMPTZ, so it needs
///          AT TIME ZONE 'UTC' to reach a plain UTC value before
///          to_char.  AQMS lddate is already TIMESTAMP WITHOUT TIME ZONE
///          holding UTC - per its own default of
///          CURRENT_TIMESTAMP AT TIME ZONE 'UTC', the same convention
///          stationQueries relies on - so AT TIME ZONE 'UTC' here would
///          RE-interpret it and convert it again.  Measured on 18.4 with
///          the server in America/Denver: 20:43:26 comes back as
///          14:43:26Z with it, and correctly as 20:43:26Z without.
///
/// @note to_timestamp gives a timestamptz; AT TIME ZONE 'UTC' brings it
///       back to the plain timestamp lddate actually is, so the
///       comparison is between like and like rather than through the
///       server's zone.  Same reasoning as the to_char above, and the
///       same pattern stationQueries uses for its lddate watermark.
constexpr std::string_view EVENT_LOCK_QUERY{
    "SELECT evid, username, "
    "       to_char(lddate, 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') "
    "  FROM jasieventlock "
    " WHERE (lddate IS NULL "
    "     OR lddate > (to_timestamp($1) AT TIME ZONE 'UTC')) "
    " ORDER BY evid"};

/// Stands in for a lock whose username column is NULL.
constexpr std::string_view UNKNOWN_USER{"unknown"};
}

std::vector<EventLock>
AQMSDutyReviewBackend::Database::AQMS::queryEventLocks(
    const DB::Client &client, const std::chrono::seconds &catalogDuration)
{
    if (catalogDuration.count() <= 0)
    {
        throw std::invalid_argument("Catalog duration must be positive");
    }
    // Anchored to this process' clock, the same one the catalog window is
    // measured from, so the two windows agree by construction rather than
    // by the database host and this pod happening to keep the same time.
    const auto now
        = std::chrono::duration_cast<std::chrono::seconds>
          (std::chrono::system_clock::now().time_since_epoch());
    const auto cutoff = now - catalogDuration - ::LOCK_WINDOW_TOLERANCE;

    std::vector<EventLock> result;
    client.execute(
        [&](pqxx::connection &connection)
        {
            pqxx::work transaction(connection);
            // Gathered into a local and assigned once: the client re-runs
            // this after re-dialling a dropped connection, and appending
            // straight into the result would list every lock twice.
            std::vector<EventLock> rows;
            for (const auto &row : transaction.exec(
                     ::EVENT_LOCK_QUERY,
                     pqxx::params{static_cast<long long> (cutoff.count())}))
            {
                // A nameless lock is still a lock.  Skipping the row would
                // report the event as free, which is the one answer that
                // could have two analysts working it at once.  Imputed
                // rather than left blank so the frontend renders it like
                // any other name instead of needing a case for "".
                EventLock lock;
                lock.setEventIdentifier(row.at(0).as<std::int64_t> ());
                lock.setUser(row.at(1).is_null()
                             ? std::string {::UNKNOWN_USER}
                             : row.at(1).as<std::string> ());
                // Left unset rather than imputed when NULL.  A missing
                // name would read as an unlocked event, which is why that
                // one gets a stand-in; a missing acquisition time only
                // means we cannot say how long it has been held, and
                // inventing one would be worse than saying nothing.
                if (!row.at(2).is_null())
                {
                    lock.setAcquisitionTime(row.at(2).as<std::string> ());
                }
                rows.push_back(std::move(lock));
            }
            transaction.commit();
            result = std::move(rows);
        },
        ::EVENT_LOCK_QUERY);
    return result;
}

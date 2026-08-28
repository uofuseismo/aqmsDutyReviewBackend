#include <vector>
#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"

using namespace AQMSDutyReviewBackend::Database::AQMS;

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<EventLock> &locks)
{
    // Built as an array from the start: a default-constructed value is
    // null, and an empty result must serialize to [] so the frontend can
    // iterate it without a special case.
    boost::json::array result;
    result.reserve(locks.size());
    for (const auto &lock : locks)
    {
        boost::json::object item;
        if (lock.hasEventIdentifier())
        {
            item["eventIdentifier"] = lock.getEventIdentifier();
        }
        if (lock.hasUser())
        {
            item["user"] = lock.getUser();
        }
        result.push_back(std::move(item));
    }
    return result;
}

boost::json::value
AQMSDutyReviewBackend::Database::AQMS::toJSON(
    const std::vector<Station> &stations)
{
    boost::json::array result;
    result.reserve(stations.size());
    for (const auto &station : stations)
    {
        boost::json::object item;
        if (station.hasNetwork()){item["network"] = station.getNetwork();}
        if (station.hasName()){item["name"] = station.getName();}
        // Absent rather than zero: station_data allows a null position,
        // and a zero would put the station in the Gulf of Guinea.
        if (station.hasLatitude()){item["latitude"] = station.getLatitude();}
        if (station.hasLongitude())
        {
            item["longitude"] = station.getLongitude();
        }
        if (station.hasStartAndEndTime())
        {
            const auto [onDate, offDate] = station.getStartAndEndTime();
            // Seconds since the epoch, UTC, as they came out of the
            // database - the frontend formats them.
            item["onDate"] = onDate.count();
            item["offDate"] = offDate.count();
        }
        // loadTime is deliberately absent.  It is lddate, and it exists so
        // the poller can ask for only the rows that changed since it last
        // looked.  The frontend lifts the network and station off this,
        // asks where that station was when the event happened, and plots
        // it - none of which the load date bears on.
        result.push_back(std::move(item));
    }
    return result;
}

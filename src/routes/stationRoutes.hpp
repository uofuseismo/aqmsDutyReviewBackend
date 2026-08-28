#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_STATION_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_STATION_ROUTES_HPP
#include <optional>
#include <string>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/hash.hpp"
#include "routeContext.hpp"

namespace
{

/// @brief Registers the station routes.
inline void registerStationRoutes(crow::SimpleApp &app,
                                  const RouteContext &context)
{
    /// @brief Fetches the stations and serializes them.
    /// @result The JSON, or nullopt if AQMS could not be reached.
    /// @note Shared by both routes below so the two cannot disagree: a
    ///       hash computed over a different serialization than the body
    ///       would have clients re-downloading forever, or never.
    /// @note This is the seam the cache goes behind later.  The poller
    ///       prefetches, this asks the cache and falls through to the
    ///       database on a miss or a forced refresh - and neither route
    ///       changes.
    static const auto fetchStationsJSON
        = [](const RouteContext &routeContext)
          -> std::optional<boost::json::value>
    {
        const auto stations = routeContext.aqmsDatabase->fetchStations();
        if (!stations){return std::nullopt;}
        return AQMSDutyReviewBackend::Database::AQMS::toJSON(*stations);
    };

    ::authorizedRoute(
        app, "/station-information", ::readOnlyRequirement, context,
        [&context](const crow::request &,
                   const AQMSDutyReviewBackend::Auth::JSONWebToken::Claims
                       &identity) -> crow::response
        {
            SPDLOG_LOGGER_INFO(context.logger, "Getting stations for {}",
                               identity.user);
            const auto stations = fetchStationsJSON(context);
            if (!stations)
            {
                // The query takes no arguments, so the only way it fails
                // is AQMS being unreachable - this backend's problem to
                // report, not something the caller can fix by asking
                // differently.
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch stations for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            return ::makeDataResponse(
                200,
                "Found " + std::to_string(stations->as_array().size())
                         + " station epochs",
                *stations);
        });

    ::authorizedRoute(
        app, "/station-information-hash", ::readOnlyRequirement, context,
        [&context](const crow::request &,
                   const AQMSDutyReviewBackend::Auth::JSONWebToken::Claims
                       &identity) -> crow::response
        {
            SPDLOG_LOGGER_DEBUG(context.logger,
                                "{} requesting stations hash...",
                                identity.user);
            const auto stations = fetchStationsJSON(context);
            if (!stations)
            {
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch stations for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            // Hashed over the same serialization the body route sends,
            // which is why both go through fetchStationsJSON.
            boost::json::object payload;
            payload["hash"] = AQMSDutyReviewBackend::hash(
                                  boost::json::serialize(*stations));
            return ::makeDataResponse(200, "Station information hash",
                                      std::move(payload));
        });
}

}
#endif

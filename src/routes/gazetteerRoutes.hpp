#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_GAZETTEER_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_GAZETTEER_ROUTES_HPP
#include <optional>
#include <string>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/quarry.hpp"
#include "aqmsDutyReviewBackend/hash.hpp"
#include "routeContext.hpp"

namespace
{

/// @brief Registers the gazetteer routes.
/// @note Nested as /gazetteer/quarries rather than flat, because the
///       gazetteer is not the quarries.  gazetteerpt holds 4302 points in
///       the archive and only 494 of them join gazetteerquarry, so "gazetteer"
///       alone would name the whole table after one of the things in it -
///       and the next point type would have nowhere to go without moving
///       a url the frontend had already learned.
inline void registerGazetteerRoutes(crow::SimpleApp &app,
                                    const RouteContext &context)
{
    /// @brief Fetches the quarries and serializes them.
    /// @result The JSON, or nullopt if AQMS could not be reached.
    /// @note Shared by both routes below so the two cannot disagree: a
    ///       hash computed over a different serialization than the body
    ///       would have clients re-downloading forever, or never.
    /// @note This is the seam the cache goes behind later.  The poller
    ///       prefetches, this asks the cache and falls through to the
    ///       database on a miss or a forced refresh - and neither route
    ///       changes.
    static const auto fetchQuarriesJSON
        = [](const RouteContext &routeContext)
          -> std::optional<boost::json::value>
    {
        const auto quarries = routeContext.aqmsDatabase->fetchQuarries();
        if (!quarries){return std::nullopt;}
        return AQMSDutyReviewBackend::Database::AQMS::toJSON(*quarries);
    };

    ::authorizedRoute(
        app, "/gazetteer/quarries", "gazetteer-quarries",
        ::readOnlyRequirement, context,
        [&context](const crow::request &,
                   const AQMSDutyReviewBackend::Auth::JSONWebToken::Claims
                       &identity) -> crow::response
        {
            SPDLOG_LOGGER_INFO(context.logger, "Getting quarries for {}",
                               identity.user);
            const auto quarries = fetchQuarriesJSON(context);
            if (!quarries)
            {
                // The query takes no arguments, so the only way it fails
                // is AQMS being unreachable - this backend's problem to
                // report, not something the caller can fix by asking
                // differently.
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch quarries for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            return ::makeDataResponse(
                200,
                "Found " + std::to_string(quarries->as_array().size())
                         + " quarry epochs",
                *quarries);
        });

    ::authorizedRoute(
        app, "/gazetteer/quarries-hash", "gazetteer-quarries-hash",
        ::readOnlyRequirement, context,
        [&context](const crow::request &,
                   const AQMSDutyReviewBackend::Auth::JSONWebToken::Claims
                       &identity) -> crow::response
        {
            SPDLOG_LOGGER_DEBUG(context.logger,
                                "{} requesting quarries hash...",
                                identity.user);
            const auto quarries = fetchQuarriesJSON(context);
            if (!quarries)
            {
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch quarries for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            // Hashed over the same serialization the body route sends,
            // which is why both go through fetchQuarriesJSON.
            boost::json::object payload;
            payload["hash"] = AQMSDutyReviewBackend::hash(
                                  boost::json::serialize(*quarries));
            return ::makeDataResponse(200, "Quarry information hash",
                                      std::move(payload));
        });
}

}
#endif

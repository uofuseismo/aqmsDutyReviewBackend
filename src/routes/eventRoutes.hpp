#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_EVENT_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_EVENT_ROUTES_HPP
#include <cstdint>
#include <string>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/aqms/eventSummary.hpp"
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "routeContext.hpp"

namespace
{

/// @brief Registers the event routes.
/// @note The routes carrying a url parameter still use CROW_ROUTE and do
///       their own authorization: the macro wants a string literal, and
///       their handler signature depends on the parameter type, so
///       ::authorizedRoute cannot express them.  They are the exception,
///       not the pattern.
inline void registerEventRoutes(crow::SimpleApp &app,
                                const RouteContext &context)
{
    using Claims = AQMSDutyReviewBackend::Auth::JSONWebToken::Claims;

    ::authorizedRoute(
        app, "/event-information/locks", ::readOnlyRequirement, context,
        [&context](const crow::request &,
                   const Claims &identity) -> crow::response
        {
            SPDLOG_LOGGER_INFO(context.logger,
                               "Getting database locks for {}",
                               identity.user);
            // Never cached: a lock's whole purpose is to say who is
            // working an event right now, and a cached answer would be
            // exactly the stale one that puts two analysts on one event.
            const auto locks = context.aqmsDatabase->getLockedEvents(context.catalogDuration);
            if (!locks)
            {
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch event locks for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            return ::makeDataResponse(
                200,
                std::to_string(locks->size()) + " locked event(s)",
                AQMSDutyReviewBackend::Database::AQMS::toJSON(*locks));
        });

    ::authorizedRoute(
        app, "/event-information/catalog", ::readOnlyRequirement, context,
        [&context](const crow::request &,
                   const Claims &identity) -> crow::response
        {
            SPDLOG_LOGGER_INFO(context.logger, "{} requesting catalog",
                               identity.user);
            const auto catalog
                = context.aqmsDatabase->getCatalog(context.catalogDuration);
            if (!catalog)
            {
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch the catalog for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            // The hash travels with the catalog rather than being
            // recomputed here, so /catalog-hash cannot drift from it.
            auto jsonCatalog
                = AQMSDutyReviewBackend::Database::AQMS::toJSON(*catalog).first;
            return ::makeDataResponse(
                200,
                "Found " + std::to_string(catalog->size()) + " event(s)",
                std::move(jsonCatalog));
        });

    ::authorizedRoute(
        app, "/event-information/catalog-hash", ::readOnlyRequirement,
        context,
        [&context](const crow::request &,
                   const Claims &identity) -> crow::response
        {
            SPDLOG_LOGGER_INFO(context.logger,
                               "{} requesting catalog hash",
                               identity.user);
            // TODO should be reading from db
            const auto catalog
                = context.aqmsDatabase->getCatalog(context.catalogDuration);
            if (!catalog)
            {
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch the catalog for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            // The hash comes back with the catalog, so this and /catalog
            // cannot disagree about what the client is comparing.
            auto hash
                = AQMSDutyReviewBackend::Database::AQMS::toJSON(*catalog).second;
            boost::json::object payload;
            payload["hash"] = std::move(hash);
            return ::makeDataResponse(200, "Catalog hash", std::move(payload));
        });

    CROW_ROUTE(app, "/waveforms-hash/<int>")
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        auto authorization = ::authorizeRoute(request, *context.authenticator,
                                              ::readOnlyRequirement,
                                              context.logger);
        if (!authorization){return std::move(*authorization.rejection);}
        SPDLOG_LOGGER_DEBUG(context.logger,
                            "{} requesting waveforms hash for {}...",
                            authorization.identity->user, eventIdentifier);
        return crow::response(200);
    });
}

}
#endif

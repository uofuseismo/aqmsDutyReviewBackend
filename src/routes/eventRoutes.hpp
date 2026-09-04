#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_EVENT_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_EVENT_ROUTES_HPP
#include <cstdint>
#include <string>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
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

    // A url parameter, so CROW_ROUTE and an explicit authorizeRoute
    // rather than ::authorizedRoute - the exception this file's header
    // comment describes.
    //
    // The <int> cannot swallow the literal routes above it.  Crow matches
    // a static segment in preference to a parameter, and "locks" is not an
    // integer in any case, so /event-information/locks and
    // /event-information/catalog still reach their own handlers.
    CROW_ROUTE(app, "/event-information/<int>")
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        auto authorization = ::authorizeRoute(request, *context.authenticator,
                                              ::readOnlyRequirement,
                                              context.logger);
        if (!authorization){return std::move(*authorization.rejection);}
        SPDLOG_LOGGER_INFO(context.logger, "{} requesting event {}",
                           authorization.identity->user, eventIdentifier);
        const auto event = context.aqmsDatabase->getEvent(eventIdentifier);
        if (!event)
        {
            SPDLOG_LOGGER_ERROR(context.logger,
                                "Could not fetch event {} for {}",
                                eventIdentifier,
                                authorization.identity->user);
            return ::makeMessageResponse(
                500,
                "Could not reach the AQMS database - try again shortly");
        }
        // An empty optional inside a good expected is "no such event",
        // which is an answer rather than a failure - hence 404 and not
        // 500.  A duty analyst can hold a link to an event that has since
        // been merged into another one.
        if (!event->has_value())
        {
            return ::makeMessageResponse(
                404, "No event " + std::to_string(eventIdentifier));
        }
        return ::makeDataResponse(
            200,
            "Found event " + std::to_string(eventIdentifier),
            AQMSDutyReviewBackend::Database::AQMS::toJSON(**event));
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

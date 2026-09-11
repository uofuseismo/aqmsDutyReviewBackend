#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_ALARM_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_ALARM_ROUTES_HPP
#include <cstdint>
#include <string>
#include <boost/json/value.hpp>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/queries/alarmQueries.hpp"
#include "routeContext.hpp"

namespace
{

/// @brief Registers the alarm routes.
inline void registerAlarmRoutes(crow::SimpleApp &app,
                                const RouteContext &context)
{
    // A url parameter, so CROW_ROUTE and an explicit authorizeRoute rather
    // than ::authorizedRoute.
    CROW_ROUTE(app, "/alarms/<int>")
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        // One name for every event, not one per identifier.
        ::RouteTimer timer{"alarms"};
        auto authorization = ::authorizeRoute(request, *context.authenticator,
                                              ::readOnlyRequirement,
                                              context.logger);
        if (!authorization)
        {
            return timer.finish(std::move(*authorization.rejection));
        }
        SPDLOG_LOGGER_INFO(context.logger, "{} requesting alarms for event {}",
                           authorization.identity->user, eventIdentifier);
        const auto alarms = context.aqmsDatabase->getAlarms(eventIdentifier);
        if (!alarms)
        {
            SPDLOG_LOGGER_ERROR(context.logger,
                                "Could not gather alarms for event {} for {}",
                                eventIdentifier,
                                authorization.identity->user);
            return timer.finish(::makeMessageResponse(
                500,
                "Could not reach the AQMS database - try again shortly"));
        }
        // 200 with an empty array, never 404.  An event with no alarms
        // and an event that does not exist look identical here, and the
        // client does not need them told apart - it draws an empty list
        // either way.  The case that DOES matter, every database being
        // unreachable, is already a 500 above: that is a "try again
        // shortly", not an empty history.
        return timer.finish(::makeDataResponse(
            200,
            "Found " + std::to_string(alarms->size())
                     + " alarm action(s) for event "
                     + std::to_string(eventIdentifier),
            AQMSDutyReviewBackend::Database::AQMS::toJSON(*alarms)));
    });
}

}
#endif

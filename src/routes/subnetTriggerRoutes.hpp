#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_SUBNET_TRIGGER_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_SUBNET_TRIGGER_ROUTES_HPP
#include <string>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/subnetTrigger.hpp"
#include "routeContext.hpp"

namespace
{

/// @brief Registers the subnet trigger routes.
inline void registerSubnetTriggerRoutes(crow::SimpleApp &app,
                                        const RouteContext &context)
{
    // The same window as the catalog, so a trigger and the events around
    // it age off the analyst's screen together.
    ::authorizedRoute(
        app, "/subnet-triggers", "subnet-triggers", ::readOnlyRequirement,
        context,
        [&context](const crow::request &,
                   const AQMSDutyReviewBackend::Auth::JSONWebToken::Claims
                       &identity) -> crow::response
        {
            SPDLOG_LOGGER_INFO(context.logger,
                               "Getting subnet triggers for {}",
                               identity.user);
            const auto triggers
                = context.aqmsDatabase->getSubnetTriggers(
                      context.catalogDuration);
            if (!triggers)
            {
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "Could not fetch subnet triggers for {}",
                                    identity.user);
                return ::makeMessageResponse(
                    500,
                    "Could not reach the AQMS database - try again shortly");
            }
            return ::makeDataResponse(
                200,
                "Found " + std::to_string(triggers->size())
                         + " subnet trigger(s)",
                AQMSDutyReviewBackend::Database::AQMS::toJSON(*triggers));
        });
}

}
#endif

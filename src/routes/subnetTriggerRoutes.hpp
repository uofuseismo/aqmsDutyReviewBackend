#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_SUBNET_TRIGGER_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_SUBNET_TRIGGER_ROUTES_HPP
#include <cstdint>
#include <string>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "aqmsDutyReviewBackend/database/aqms/subnetTrigger.hpp"
#include "routeContext.hpp"
#include "waveformRoutes.hpp"

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

    // Every channel archived for the trigger, not just picked ones - a
    // trigger has no picks, and deciding whether it is an earthquake means
    // looking at what every station recorded.
    ::describeRoute("GET", "/subnet-triggers/<int>/waveforms",
                    "subnet-trigger-waveforms", ::readOnlyRequirement);
    CROW_ROUTE(app, "/subnet-triggers/<int>/waveforms")
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        return ::timedRoute("subnet-trigger-waveforms",
                            [&]() -> crow::response
        {
            auto authorization
                = ::authorizeRoute(request, *context.authenticator,
                                   ::readOnlyRequirement, context.logger,
                                   "subnet-trigger-waveforms");
            if (!authorization){return std::move(*authorization.rejection);}
            SPDLOG_LOGGER_INFO(context.logger,
                               "{} getting waveforms for subnet trigger {}",
                               authorization.identity->user, eventIdentifier);
            namespace AQMS = AQMSDutyReviewBackend::Database::AQMS;
            const auto options = ::readWaveformOptions(request);
            // Verticals by default: they are what tells an earthquake from
            // noise, and they are a third of the payload.
            const auto verticalOnly
                = ::readFlag(request, "verticalOnly", true);

            // Checked, because the fetch below takes any event: without
            // this, a real event's identifier here would cost every
            // channel it has.  A trigger reclassified since the list was
            // loaded lands here too, and the message says so.
            const auto event
                = context.aqmsDatabase->getEvent(eventIdentifier);
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
            if (!event->has_value()
             || !(*event)->hasEventType()
             || (*event)->getEventType() != AQMS::Event::EventType::SubnetTrigger)
            {
                return ::makeMessageResponse(
                    404, "No subnet trigger " + std::to_string(eventIdentifier)
                       + " - it may have been deleted or reclassified");
            }

            const auto waveforms
                = context.aqmsDatabase->fetchAllWaveforms(eventIdentifier,
                                                          verticalOnly);
            if (!waveforms)
            {
                // Two different faults, said differently: the database
                // being unreachable is worth retrying, a query it refuses
                // is not.
                if (waveforms.error() == AQMS::Database::QueryError::QueryFailed)
                {
                    return ::makeMessageResponse(
                        500, "The AQMS waveform query failed - this is a "
                             "backend fault, not a missing waveform");
                }
                return ::makeMessageResponse(
                    500, "Could not reach the AQMS database - try again shortly");
            }
            if (waveforms->empty())
            {
                // Why each file was skipped is in the log.
                return ::makeMessageResponse(
                    404, "No waveform data for subnet trigger "
                       + std::to_string(eventIdentifier));
            }
            return ::makeWaveformResponse(
                *waveforms, options, authorization.identity->user,
                "subnet trigger " + std::to_string(eventIdentifier),
                context.logger.get());
        });
    });
}

}
#endif

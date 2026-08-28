#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_ACTION_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_ACTION_ROUTES_HPP
#include <cstdint>
#include <crow/app.h>
#include "routeContext.hpp"

namespace
{

/// The requirement for acting on an event.  Accepting or cancelling
/// changes what the rest of AQMS does about it, so a reader may not.
constexpr AQMSDutyReviewBackend::Auth::Requirement readWriteRequirement
{
    AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadWrite,
    false // Require password
};

/// @brief Registers the analyst action routes.
/// @note Both carry a url parameter, so they use CROW_ROUTE and authorize
///       inline - see the note in eventRoutes.hpp.
inline void registerActionRoutes(crow::SimpleApp &app,
                                 const RouteContext &context)
{
    CROW_ROUTE(app, "/actions/accept/<int>")
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        auto authorization = ::authorizeRoute(request, *context.authenticator,
                                              ::readWriteRequirement,
                                              context.logger);
        if (!authorization){return std::move(*authorization.rejection);}
        SPDLOG_LOGGER_INFO(context.logger, "{} accepting event {}",
                           authorization.identity->user, eventIdentifier);
        return crow::response(200);
    });

    CROW_ROUTE(app, "/actions/cancel/<int>")
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        auto authorization = ::authorizeRoute(request, *context.authenticator,
                                              ::readWriteRequirement,
                                              context.logger);
        if (!authorization){return std::move(*authorization.rejection);}
        SPDLOG_LOGGER_INFO(context.logger, "{} cancelling event {}",
                           authorization.identity->user, eventIdentifier);
        return crow::response(200);
    });
}

}
#endif

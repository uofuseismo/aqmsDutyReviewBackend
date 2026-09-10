#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_ACTION_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_ACTION_ROUTES_HPP
#include <expected>
#include <memory>
#include <string>
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


/// @brief Turns an action outcome into a response.
/// @note InvalidPermissions here is the DATABASE's answer, not the
///       caller's - the route already refused a read-only user before
///       getting this far.  It means the AQMS account this backend
///       connects with cannot write, which is a deployment fault and so a
///       500 rather than a 403: telling the analyst they lack permission
///       would send them to the wrong person.
[[nodiscard]] crow::response actionResponse(
    const std::expected<void,
        AQMSDutyReviewBackend::Database::AQMS::Database::ActionError> &result,
    const std::string &verb,
    const int64_t eventIdentifier,
    const std::shared_ptr<spdlog::logger> &logger)
{
    using ActionError
        = AQMSDutyReviewBackend::Database::AQMS::Database::ActionError;
    if (result)
    {
        return ::makeMessageResponse(
            200, "Event " + std::to_string(eventIdentifier) + " " + verb);
    }
    if (result.error() == ActionError::DoesNotExist)
    {
        // AQMS answered, and answered no.  For a cancel this also covers
        // "no database would take it", which is worth saying plainly
        // rather than as a generic failure.
        return ::makeMessageResponse(
            404, "AQMS would not " + verb.substr(0, verb.size() - 2)
               + " event " + std::to_string(eventIdentifier)
               + " - it may not exist");
    }
    if (result.error() == ActionError::InvalidPermissions)
    {
        SPDLOG_LOGGER_ERROR(logger,
                            "The AQMS account cannot write - event {} was "
                            "not {}",
                            eventIdentifier, verb);
        return ::makeMessageResponse(
            500, "This backend cannot write to AQMS - tell an operator");
    }
    return ::makeMessageResponse(
        500, "Could not reach the AQMS database - try again shortly");
}

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
        return ::actionResponse(context.aqmsDatabase->accept(eventIdentifier),
                                "accepted", eventIdentifier, context.logger);
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
        return ::actionResponse(context.aqmsDatabase->cancel(eventIdentifier),
                                "cancelled", eventIdentifier, context.logger);
    });
}

}
#endif

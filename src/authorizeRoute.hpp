#ifndef AQMS_DUTY_REVIEW_BACKEND_AUTHORIZE_ROUTE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_AUTHORIZE_ROUTE_HPP
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <crow/http_request.h>
#include <crow/http_response.h>
#include <crow/json.h>
#include "aqmsDutyReviewBackend/auth/authNZ.hpp"
#include "aqmsDutyReviewBackend/auth/authenticator.hpp"
#include "aqmsDutyReviewBackend/auth/authorization.hpp"
#include "aqmsDutyReviewBackend/auth/jsonWebToken.hpp"

namespace
{

/// @brief The Crow half of the per-request check: what a route handler
///        gets back from ::authorizeRoute.
///
/// Either the caller is allowed and \c identity says who they are, or
/// they are not and \c rejection is the response to return.  Exactly one
/// is populated, and the bool conversion is the one to branch on:
///
///     auto authorization
///         = ::authorizeRoute(request, authNZ,
///                            {Permissions::ReadWrite}, logger);
///     if (!authorization){return *authorization.rejection;}
///     const auto &user = authorization.identity->user;
///
struct RouteAuthorization
{
    /// Who the caller is.  Populated only when they are allowed.
    std::optional<AQMSDutyReviewBackend::Auth::JSONWebToken::Claims> identity;
    /// The response to return.  Populated only when they are not.
    std::optional<crow::response> rejection;

    /// @result True if the caller may proceed.
    explicit operator bool() const noexcept
    {
        return identity.has_value();
    }
};

/// @brief Builds the body sent to a rejected caller.
/// @note Deliberately vague.  The verdict's own reason separates an
///       unknown user from a wrong password and a missing header from an
///       unsupported scheme; the client is told none of that, because
///       those distinctions are what let someone enumerate valid user
///       names.  The detail goes to the log instead.
[[nodiscard]] inline std::string authorizationErrorBody(const int statusCode)
{
    crow::json::wvalue body;
    if (statusCode == 400)
    {
        body["error"] = "Malformed authorization header";
    }
    else if (statusCode == 403)
    {
        body["error"] = "Insufficient permissions";
    }
    else if (statusCode == 500)
    {
        body["error"] = "Internal server error";
    }
    else
    {
        body["error"] = "Unauthorized";
    }
    return body.dump();
}

/// @brief Runs a route's requirement against a request.
/// @param[in] request      The incoming request.
/// @param[in] authNZ       The authentication and authorization utility.
/// @param[in] requirement  What this route demands of its caller.  A
///                         route needing more than a read-only bearer
///                         says so here; the change-password route sets
///                         requirePassword.
/// @param[in] logger       Where the reason for a denial is recorded.
/// @result The identity, or the response to return.
/// @note This is the single place a route touches authentication.  The
///       point is that adding a route cannot accidentally skip the
///       check: the handler has no identity to work with until this has
///       run, so forgetting it does not compile into something that
///       silently serves everyone.
[[nodiscard]] inline RouteAuthorization authorizeRoute(
    const crow::request &request,
    const AQMSDutyReviewBackend::Auth::AuthNZ &authNZ,
    const AQMSDutyReviewBackend::Auth::Requirement &requirement,
    const std::shared_ptr<spdlog::logger> &logger)
{
    namespace Auth = AQMSDutyReviewBackend::Auth;

    const auto verdict
        = authNZ.authorize(request.get_header_value("Authorization"),
                           requirement);
    if (verdict.isAllowed())
    {
        SPDLOG_LOGGER_DEBUG(logger, "{}", verdict.reason);
        return RouteAuthorization {verdict.identity, std::nullopt};
    }

    const auto statusCode = verdict.statusCode();
    // A 500 is this backend's fault and belongs at error level; the rest
    // are the client's and would otherwise fill the log with noise from
    // anybody probing the port.
    if (verdict.status == Auth::Authorization::Status::ServerError)
    {
        SPDLOG_LOGGER_ERROR(logger, "Authorization failed: {}",
                            verdict.reason);
    }
    else
    {
        SPDLOG_LOGGER_INFO(logger, "Authorization denied ({}): {}",
                           statusCode, verdict.reason);
    }

    crow::response response{statusCode, ::authorizationErrorBody(statusCode)};
    response.set_header("Content-Type", "application/json");
    // Naming the scheme matters on the password-only routes: challenging
    // with Bearer there would have the client re-send the token that was
    // just refused, forever.
    const auto challenge = verdict.challenge();
    if (challenge != std::nullopt)
    {
        response.set_header("WWW-Authenticate",
                            Auth::schemeToString(*challenge)
                          + std::string {" realm=\"aqmsDutyReviewBackend\""});
    }
    return RouteAuthorization {std::nullopt, std::move(response)};
}

}
#endif

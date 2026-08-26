#ifndef AQMS_DUTY_REVIEW_BACKEND_AUTHORIZE_ROUTE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_AUTHORIZE_ROUTE_HPP
#include <exception>
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

/// @brief Builds the response sent to a caller who is turned away.
/// @param[in] statusCode  The HTTP status to return.
/// @param[in] challenge   The scheme to name in WWW-Authenticate, or
///                        nullopt for the verdicts that do not call for
///                        one.
/// @note Naming the scheme matters on the password-only routes:
///       challenging with Bearer there would have the client re-send the
///       token that was just refused, forever.
[[nodiscard]] inline crow::response makeAuthorizationRejection(
    const int statusCode,
    const std::optional<AQMSDutyReviewBackend::Auth::Scheme> challenge)
{
    crow::response response{statusCode, ::authorizationErrorBody(statusCode)};
    response.set_header("Content-Type", "application/json");
    if (challenge != std::nullopt)
    {
        response.set_header(
            "WWW-Authenticate",
            AQMSDutyReviewBackend::Auth::schemeToString(*challenge)
          + std::string {" realm=\"aqmsDutyReviewBackend\""});
    }
    return response;
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

    // Reading the header is the one step of the check that sits outside
    // AuthNZ::authorize's noexcept guarantee, so it does not get to be
    // the thing that escapes.  Crow hashes the key through the global
    // locale - std::locale's constructor and the use_facet inside the
    // locale-aware std::toupper can both throw - and copying the value
    // out allocates.
    //
    // A failure here answers 400, the same as a header that parsed but
    // made no sense: every way of failing to read a caller's
    // Authorization header looks identical from the outside, so nobody
    // gets to tell from a response whether it was their header or this
    // server that was unwell.  The cost is that a genuine fault is
    // reported as a client error, which is why the log line below stays
    // at error level - it is the only place that distinction survives.
    std::string authorization;
    try
    {
        authorization = request.get_header_value("Authorization");
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(logger,
                            "Could not read the authorization header "
                            "because {}", std::string {e.what()});
        return RouteAuthorization
               {std::nullopt,
                ::makeAuthorizationRejection(400, std::nullopt)};
    }
    catch (...)
    {
        // Not everything throwable derives from std::exception, and the
        // point of this block is that nothing escapes.
        SPDLOG_LOGGER_ERROR(logger,
                            "Could not read the authorization header");
        return RouteAuthorization
               {std::nullopt,
                ::makeAuthorizationRejection(400, std::nullopt)};
    }

    const auto verdict = authNZ.authorize(authorization, requirement);
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
    return RouteAuthorization
           {std::nullopt,
            ::makeAuthorizationRejection(statusCode, verdict.challenge())};
}

}
#endif

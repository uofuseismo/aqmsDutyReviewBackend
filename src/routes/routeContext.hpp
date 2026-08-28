#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_ROUTE_CONTEXT_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_ROUTE_CONTEXT_HPP
#include <chrono>
#include <memory>
#include <string>
#include <utility>
#include <spdlog/logger.h>
#include <crow/app.h>
#include <crow/common.h>
#include <crow/http_request.h>
#include <crow/http_response.h>
#include "aqmsDutyReviewBackend/auth/authNZ.hpp"
#include "aqmsDutyReviewBackend/auth/authenticator.hpp"
#include "aqmsDutyReviewBackend/auth/jsonWebToken.hpp"
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/drp/userStore.hpp"
#include "authorizeRoute.hpp"

namespace
{

/// @brief Everything the route handlers share.
///
/// Assembled once in main and handed to each family of routes, so adding a
/// route does not mean threading another capture through main's lambda
/// soup.
///
/// @warning The two raw pointers are NOT owned.  main owns those objects
///          and outlives app.run(), which is the only reason this is safe;
///          nothing here extends their lifetime.
struct RouteContext
{
    /// Who the caller is and what they may do.
    const AQMSDutyReviewBackend::Auth::AuthNZ *authenticator{nullptr};
    /// The AQMS databases.
    const AQMSDutyReviewBackend::Database::AQMS::Database *aqmsDatabase
        {nullptr};
    /// This backend's own users.
    std::shared_ptr<AQMSDutyReviewBackend::Database::DRP::UserStore> users;
    /// Where the handlers log.
    std::shared_ptr<spdlog::logger> logger;
    /// How long a new account's temporary password stays usable.
    std::chrono::seconds newAccountLifetime{std::chrono::hours {24*7}};
    /// How long a reset password stays usable.
    std::chrono::seconds passwordResetLifetime{std::chrono::hours {24}};
};

/// @brief Registers a route that only an authorized caller reaches.
///
/// @param[in] app          The application.
/// @param[in] url          The route.
/// @param[in] method       The HTTP method.
/// @param[in] requirement  What the caller must hold.
/// @param[in] context      The shared dependencies.  Captured by
///                         reference, so it must outlive the app.
/// @param[in] handler      Called with the request and the verified
///                         identity, and only when the caller cleared the
///                         bar.
///
/// @note The point is not the eight lines it saves.  The handler is handed
///       an identity it cannot obtain any other way, so a route cannot be
///       written that forgets to check - the check is not something to
///       remember, it is the only way in.
///
/// @note crow's route_dynamic rather than the CROW_ROUTE macro, because
///       the macro wants a string literal and this takes the url as an
///       argument.  Routes with url parameters - /waveforms/<int> - still
///       want the macro, since their handler signature depends on the
///       parameter types.
template<typename Handler>
void authorizedRoute(crow::SimpleApp &app,
                     const std::string &url,
                     const crow::HTTPMethod method,
                     const AQMSDutyReviewBackend::Auth::Requirement &requirement,
                     const RouteContext &context,
                     Handler handler)
{
    app.route_dynamic(url)
      .methods(method)
      ([&context, requirement, handler](const crow::request &request)
       -> crow::response
       {
           auto authorization = ::authorizeRoute(request,
                                                 *context.authenticator,
                                                 requirement,
                                                 context.logger);
           if (!authorization){return std::move(*authorization.rejection);}
           return handler(request, *authorization.identity);
       });
}

/// @brief Registers an authorized GET route.
template<typename Handler>
void authorizedRoute(crow::SimpleApp &app,
                     const std::string &url,
                     const AQMSDutyReviewBackend::Auth::Requirement &requirement,
                     const RouteContext &context,
                     Handler handler)
{
    ::authorizedRoute(app, url, crow::HTTPMethod::GET, requirement, context,
                      std::move(handler));
}

/// The requirement most read routes use.
constexpr AQMSDutyReviewBackend::Auth::Requirement readOnlyRequirement
{
    AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
    false // Require password
};

/// The requirement the user-management routes use.
constexpr AQMSDutyReviewBackend::Auth::Requirement administratorRequirement
{
    AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::Administrator,
    false // Require password
};

}
#endif

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
#include "aqmsDutyReviewBackend/auth/password.hpp"
#include "aqmsDutyReviewBackend/database/aqms/catalogCache.hpp"
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/drp/userStore.hpp"
#include "authorizeRoute.hpp"
#include "routeMetrics.hpp"

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
    /// What a user-chosen password must look like.  Enforced by the
    /// change-password route and published by the requirements route, so
    /// the frontend and the backend cannot disagree about the rules.
    /// Grouped with the two lifetimes above because all three come from
    /// [UserManagement] and main initializes this aggregate positionally.
    AQMSDutyReviewBackend::Auth::PasswordPolicy passwordPolicy;
    /// What the routes spend hashing a password.  The same value the
    /// authenticator was built with - a route hashing at one cost while
    /// the authenticator judges staleness at another would make every
    /// subsequent login rehash.
    AQMSDutyReviewBackend::Auth::PasswordHashingCost passwordHashingCost;
    /// Holds the serialized catalog between requests so that asking
    /// whether it changed does not rebuild it.  Shared, and mutated
    /// through the pointer - the context itself is const in the handlers.
    /// @note Not owned by the context in spirit; main owns it and it
    ///       outlives app.run(), like the two raw pointers above.
    std::shared_ptr<AQMSDutyReviewBackend::Database::AQMS::CatalogCache>
        catalogCache;
    /// How far back the catalog reaches, and with it how far back a lock
    /// is worth reporting.  One week by default; set from
    /// General.catalogDurationInDays.
    /// @note main initializes this aggregate positionally, so this stays
    ///       last - and the default is only reached if main stops passing
    ///       it, which is how it silently sat at a fortnight before.
    std::chrono::seconds catalogDuration{std::chrono::hours {24*7}};
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
/// @param[in] metricName  What this route is called in the metrics.
///                        Hyphenated and url-free - "event-information",
///                        not "/event-information/<int>" - matching the
///                        auth-login example in metricsSingleton.
/// @note Named rather than derived from the url, and deliberately.  A url
///       carrying an event identifier would mint one metric per event and
///       bury the route's behaviour under a million rows of one hit each,
///       which is the cardinality mistake that makes a metrics backend
///       expensive and useless at the same time.  Several urls sharing one
///       name is fine and often wanted; a name per request never is.
template<typename Handler>
void authorizedRoute(crow::SimpleApp &app,
                     const std::string &url,
                     const std::string &metricName,
                     const crow::HTTPMethod method,
                     const AQMSDutyReviewBackend::Auth::Requirement &requirement,
                     const RouteContext &context,
                     Handler handler)
{
    app.route_dynamic(url)
      .methods(method)
      ([&context, requirement, handler, metricName]
       (const crow::request &request) -> crow::response
       {
           ::RouteTimer timer{metricName};
           auto authorization = ::authorizeRoute(request,
                                                 *context.authenticator,
                                                 requirement,
                                                 context.logger);
           if (!authorization)
           {
               return timer.finish(std::move(*authorization.rejection));
           }
           return timer.finish(handler(request, *authorization.identity));
       });
}

/// @brief Registers an authorized GET route.
template<typename Handler>
void authorizedRoute(crow::SimpleApp &app,
                     const std::string &url,
                     const std::string &metricName,
                     const AQMSDutyReviewBackend::Auth::Requirement &requirement,
                     const RouteContext &context,
                     Handler handler)
{
    ::authorizedRoute(app, url, metricName, crow::HTTPMethod::GET, requirement,
                      context, std::move(handler));
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

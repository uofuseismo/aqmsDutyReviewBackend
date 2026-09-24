#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_ACTION_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_ACTION_ROUTES_HPP
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <cstdint>
#include <crow/app.h>
#include "aqmsDutyReviewBackend/database/aqms/queries/actions.hpp"
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
#include "requestBody.hpp"
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


/// @brief What ::parseExpectedSolution gets back - the solution, or the
///        response to return instead.  Shaped like RequestData.
struct ExpectedSolutionRequest
{
    std::optional<AQMSDutyReviewBackend::Database::AQMS::ExpectedSolution>
        expected;
    std::optional<crow::response> rejection;

    explicit operator bool() const noexcept
    {
        return expected.has_value();
    }
};

/// @brief Reads one identifier the analyst saw.
/// @result The rejection if the field is absent or not a positive integer
///         or null; otherwise nothing, with \c identifier set or unset.
[[nodiscard]] std::optional<crow::response> readExpectedIdentifier(
    const boost::json::object &data,
    const std::string_view key,
    std::optional<std::int64_t> &identifier)
{
    const auto *value = data.if_contains(key);
    if (value != nullptr && value->is_null())
    {
        identifier.reset();
        return std::nullopt;
    }
    if (value != nullptr && value->is_int64() && value->as_int64() > 0)
    {
        identifier = value->as_int64();
        return std::nullopt;
    }
    return ::makeMessageResponse(
        400, "\"data." + std::string {key}
           + "\" is required - the identifier that was reviewed, or null "
             "if there was none");
}

/// @brief Reads what the analyst reviewed from the request body.
/// @note Every field is required.  An action that does not say what it
///       is acting on cannot be checked, and would quietly act on whatever
///       the event has become.
[[nodiscard]] ExpectedSolutionRequest parseExpectedSolution(
    const crow::request &request)
{
    auto body = ::parseRequestData(request);
    if (!body){return {std::nullopt, std::move(body.rejection)};}
    AQMSDutyReviewBackend::Database::AQMS::ExpectedSolution expected;
    if (auto rejection
            = ::readExpectedIdentifier(*body.data, "expectedPreferredOriginId",
                                       expected.preferredOriginIdentifier))
    {
        return {std::nullopt, std::move(rejection)};
    }
    if (auto rejection
            = ::readExpectedIdentifier(*body.data,
                                       "expectedPreferredMagnitudeId",
                                       expected.preferredMagnitudeIdentifier))
    {
        return {std::nullopt, std::move(rejection)};
    }
    const auto *eventType = body.data->if_contains("expectedEventType");
    const auto parsedType
        = (eventType != nullptr && eventType->is_string())
        ? AQMSDutyReviewBackend::Database::AQMS::eventTypeFromString(
              eventType->as_string())
        : std::nullopt;
    if (!parsedType)
    {
        return {std::nullopt,
                ::makeMessageResponse(
                    400, "\"data.expectedEventType\" is required - the "
                         "event type as the event detail names it, e.g. "
                         "\"earthquake\"")};
    }
    expected.eventType = *parsedType;
    return {expected, std::nullopt};
}

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
    if (result.error() == ActionError::SolutionChanged)
    {
        return ::makeMessageResponse(
            409, "Event " + std::to_string(eventIdentifier)
               + " changed while you were reviewing it - it was not "
               + verb + ".  Reload it to review the new solution");
    }
    if (result.error() == ActionError::DoesNotExist)
    {
        return ::makeMessageResponse(
            404, "Event " + std::to_string(eventIdentifier)
               + " does not exist");
    }
    if (result.error() == ActionError::Refused)
    {
        // 422 and not 409: 409 means "your view is stale, reload", and
        // reloading will not change this answer.
        const bool cancelling = (verb == "cancelled");
        return ::makeMessageResponse(
            422, std::string {cancelling ? "AQMS would not cancel event "
                                         : "AQMS would not accept event "}
               + std::to_string(eventIdentifier)
               + (cancelling
                  ? " - it may already be cancelled, or no machine holds "
                    "it to cancel"
                  : ""));
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
/// @note POST, and CROW_ROUTE defaults to GET, so the .methods() call is
///       load bearing rather than decorative.  These are not safe: accept
///       bumps the event version, and cancel posts to PCS, which is what
///       sends a cancellation message out to whoever is listening.
///
///       Idempotency is a separate question and does not license a GET -
///       PUT and DELETE are idempotent too.  What GET promises is that
///       nothing happens, and something very much happens here.  Anything
///       that follows links or prefetches - a browser, a crawler, a
///       security scanner, a chat client unfurling a pasted URL - would
///       otherwise cancel events by looking at them.
///
/// @note They ARE close to idempotent, which is worth having: accepting an
///       accepted event answers 1 without bumping the version again, and
///       cancelling a cancelled one re-posts to PCS but leaves the flag
///       where it already was.  So a retry after a timeout is safe, even
///       though a cancel retry may send a second message.
inline void registerActionRoutes(crow::SimpleApp &app,
                                 const RouteContext &context)
{
    ::describeRoute("POST", "/events/<int>/accept", "event-accept",
                    ::readWriteRequirement);
    CROW_ROUTE(app, "/events/<int>/accept")
        .methods(crow::HTTPMethod::POST)
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        return ::timedRoute("event-accept",
                            [&]() -> crow::response
        {
            auto authorization = ::authorizeRoute(request, *context.authenticator,
                                                  ::readWriteRequirement,
                                                  context.logger,
                                                  "event-accept");
            if (!authorization){return std::move(*authorization.rejection);}
            auto expected = ::parseExpectedSolution(request);
            if (!expected){return std::move(*expected.rejection);}
            SPDLOG_LOGGER_INFO(context.logger, "{} accepting event {}",
                               authorization.identity->user, eventIdentifier);
            const auto result
                = context.aqmsDatabase->accept(eventIdentifier,
                                             *expected.expected);
            return ::actionResponse(result, "accepted", eventIdentifier,
                                    context.logger);
        });
    });

    ::describeRoute("POST", "/events/<int>/cancel", "event-cancel",
                    ::readWriteRequirement);
    CROW_ROUTE(app, "/events/<int>/cancel")
        .methods(crow::HTTPMethod::POST)
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        return ::timedRoute("event-cancel",
                            [&]() -> crow::response
        {
            auto authorization = ::authorizeRoute(request, *context.authenticator,
                                                  ::readWriteRequirement,
                                                  context.logger,
                                                  "event-cancel");
            if (!authorization){return std::move(*authorization.rejection);}
            auto expected = ::parseExpectedSolution(request);
            if (!expected){return std::move(*expected.rejection);}
            SPDLOG_LOGGER_INFO(context.logger, "{} cancelling event {}",
                               authorization.identity->user, eventIdentifier);
            const auto result
                = context.aqmsDatabase->cancel(eventIdentifier,
                                             *expected.expected);
            return ::actionResponse(result, "cancelled", eventIdentifier,
                                    context.logger);
        });
    });
}

}
#endif

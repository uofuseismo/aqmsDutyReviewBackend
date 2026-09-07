#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_WAVEFORM_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_WAVEFORM_ROUTES_HPP
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include <crow/app.h>
#include "routeContext.hpp"
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/aqms/streamIdentifier.hpp"
#include "aqmsDutyReviewBackend/database/aqms/waveform.hpp"
#include "aqmsDutyReviewBackend/database/aqms/segment.hpp"
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"

namespace
{

/// @brief Registers the waveform routes.
/// @note The route carries a url parameter, so it uses CROW_ROUTE and
///       authorizes inline - see the note in eventRoutes.hpp.
inline void registerWaveformRoutes(crow::SimpleApp &app,
                                   const RouteContext &context)
{
    CROW_ROUTE(app, "/waveforms/location/<int>")
    ([&context](const crow::request &request,
                const int64_t eventIdentifier) -> crow::response
    {
        auto authorization = ::authorizeRoute(request, *context.authenticator,
                                              ::readOnlyRequirement,
                                              context.logger);
        if (!authorization){return std::move(*authorization.rejection);}
        SPDLOG_LOGGER_INFO(context.logger, "{} getting waveforms for event {}",
                           authorization.identity->user, eventIdentifier);
        // HARDWIRED, on purpose: a known event and a stream known to have
        // data for it, so the miniSEED path can be exercised end to end
        // before anything has to decide which streams an event has.  The
        // eventIdentifier from the url is deliberately ignored until then.
        namespace AQMS = AQMSDutyReviewBackend::Database::AQMS;

        // How the samples come back.  Both off by default, so a caller
        // that asks for nothing gets the plain form and nothing it did not
        // ask for changes under it.  The reply says what was actually
        // done - see the gain and deltaEncoded on every segment - because
        // delta encoding is declined on fractional samples.
        //
        // Query parameters rather than a body: this is a GET, and these
        // pick a representation of the same resource rather than changing
        // it.
        AQMS::WaveformEncoding encoding;
        const auto readFlag
            = [&request](const char *name, const bool fallback)
              {
                  const auto value = request.url_params.get(name);
                  if (value == nullptr){return fallback;}
                  const std::string text{value};
                  // "true"/"1" on, "false"/"0" off; anything else is
                  // treated as unset rather than guessed at.
                  if (text == "true" || text == "1"){return true;}
                  if (text == "false" || text == "0"){return false;}
                  return fallback;
              };
        encoding.enableDeltaEncoding
            = readFlag("enableDeltaEncoding", false);
        encoding.enableQuantization
            = readFlag("enableQuantization", false);

        constexpr int64_t hardwiredEvent{31151421};
        AQMS::StreamIdentifier streamIdentifier;
        streamIdentifier.setNetwork("UU");
        streamIdentifier.setStation("ECUT");
        streamIdentifier.setChannel("HHZ");
        streamIdentifier.setLocationCode("01");
        const std::vector<AQMS::StreamIdentifier> streamIdentifiers
            {streamIdentifier};

        const auto waveforms
            = context.aqmsDatabase->fetchWaveforms(hardwiredEvent,
                                                   streamIdentifiers);
        if (!waveforms)
        {
            // Two different faults, said differently: the database being
            // unreachable is worth retrying, a query it refuses is not.
            using QueryError
                = AQMS::Database::QueryError;
            if (waveforms.error() == QueryError::QueryFailed)
            {
                SPDLOG_LOGGER_ERROR(context.logger,
                                    "AQMS refused the waveform query for "
                                    "event {}", hardwiredEvent);
                return ::makeMessageResponse(
                    500, "The AQMS waveform query failed - this is a "
                         "backend fault, not a missing waveform");
            }
            SPDLOG_LOGGER_ERROR(context.logger,
                                "Could not fetch waveforms for event {}",
                                hardwiredEvent);
            return ::makeMessageResponse(
                500, "Could not reach the AQMS database - try again shortly");
        }
        if (waveforms->empty())
        {
            // AQMS was reached and had nothing for the stream.  The reason
            // is in the log - fetchWaveforms records why each stream was
            // skipped, which is what distinguishes "the query returned no
            // rows" from "the miniSEED would not parse".
            return ::makeMessageResponse(
                404, "No waveform data for that event and stream");
        }
        std::size_t nSegments{0};
        for (const auto &waveform : *waveforms)
        {
            nSegments = nSegments + waveform.size();
        }
        SPDLOG_LOGGER_INFO(context.logger,
                           "Returning {} waveform(s) and {} segment(s) for "
                           "{} (delta={}, quantized={})",
                           waveforms->size(), nSegments,
                           authorization.identity->user,
                           encoding.enableDeltaEncoding,
                           encoding.enableQuantization);
        return ::makeDataResponse(
            200,
            "Found " + std::to_string(waveforms->size()) + " waveform(s) in "
                     + std::to_string(nSegments) + " segment(s)",
            AQMS::toJSON(*waveforms, encoding));
    }); 
}
}

#endif

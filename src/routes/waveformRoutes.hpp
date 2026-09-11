#ifndef AQMS_DUTY_REVIEW_BACKEND_ROUTES_WAVEFORM_ROUTES_HPP
#define AQMS_DUTY_REVIEW_BACKEND_ROUTES_WAVEFORM_ROUTES_HPP
#include <algorithm>
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
#include "aqmsDutyReviewBackend/database/aqms/event.hpp"
#include "aqmsDutyReviewBackend/database/aqms/origin.hpp"
#include "aqmsDutyReviewBackend/database/aqms/arrival.hpp"
#include "aqmsDutyReviewBackend/signalProcessing/demean.hpp"
#include "aqmsDutyReviewBackend/signalProcessing/bandpass.hpp"

namespace
{

/// @brief The distinct streams an origin's arrivals were picked on.
/// @note The arrivals reaching here have already been filtered by weight -
///       the event query keeps only those AQMS did not associate at zero -
///       so these are the channels that actually contributed to the
///       location.  Note that a NULL weight counts as kept: over half the
///       associations have one in practice, and a strict "weight > 0" would
///       silently drop most of the record section.
/// @note Distinct, because two phases on one channel are two arrivals and
///       one waveform.  Ordered by the arrival order rather than sorted,
///       so the record section comes out roughly in pick order.
[[nodiscard]] std::vector<AQMSDutyReviewBackend::Database::AQMS::StreamIdentifier>
streamsFromArrivals(
    const AQMSDutyReviewBackend::Database::AQMS::Origin &origin)
{
    namespace AQMS = AQMSDutyReviewBackend::Database::AQMS;
    std::vector<AQMS::StreamIdentifier> result;
    for (const auto &arrival : origin.getArrivals())
    {
        if (!arrival.hasStreamIdentifier()){continue;}
        auto streamIdentifier = arrival.getStreamIdentifier();
        const auto alreadyHave
            = std::any_of(result.begin(), result.end(),
                          [&](const AQMS::StreamIdentifier &other)
                          {
                              return other.getNetwork()
                                         == streamIdentifier.getNetwork() &&
                                     other.getStation()
                                         == streamIdentifier.getStation() &&
                                     other.getChannel()
                                         == streamIdentifier.getChannel() &&
                                     other.getLocationCode()
                                         == streamIdentifier.getLocationCode();
                          });
        if (!alreadyHave){result.push_back(std::move(streamIdentifier));}
    }
    return result;
}

/// @brief Removes the mean and bandpass filters every segment.
///
/// @note A segment that cannot be filtered is passed through UNFILTERED
///       rather than dropped or failing the request.  The usual reason is
///       a sampling rate with no designed filter - getValidSamplingRate
///       refuses anything it cannot match, deliberately, because snapping
///       90 Hz onto the 100 Hz design would apply a filter whose corners
///       sit in the wrong place and produce a trace that looks plausible
///       and is wrong.  An unfiltered trace is honest; a
///       wrongly-filtered one is not.
///
/// @note Per SEGMENT, not per waveform.  Segments are separated by real
///       gaps in the record, so filtering across one would ring the filter
///       against a discontinuity that is not in the ground motion.
[[nodiscard]] AQMSDutyReviewBackend::Database::AQMS::Waveform
filterWaveform(const AQMSDutyReviewBackend::Database::AQMS::Waveform &waveform,
               spdlog::logger *logger)
{
    namespace AQMS = AQMSDutyReviewBackend::Database::AQMS;
    namespace SP = AQMSDutyReviewBackend::SignalProcessing;
    if (waveform.empty()){return waveform;}

    std::vector<AQMS::Segment> segments;
    segments.reserve(waveform.size());
    for (const auto &segment : waveform)
    {
        auto filtered = segment;
        try
        {
            if (segment.hasData() && segment.hasSamplingRate())
            {
                const auto samplingRate
                    = SP::getValidSamplingRate(segment.getSamplingRate());
                auto samples
                    = SP::bandpassFilter(
                          SP::demean(segment.getDataReference()),
                          samplingRate);
                // Assigned only once both steps have succeeded, so a
                // failure half way leaves the segment as it arrived.
                filtered.setData(std::move(samples));
            }
        }
        catch (const std::exception &e)
        {
            filtered = segment;
            if (logger != nullptr)
            {
                SPDLOG_LOGGER_DEBUG(logger,
                                    "Sending a segment unfiltered because {}",
                                    std::string {e.what()});
            }
        }
        segments.push_back(std::move(filtered));
    }
    AQMS::Waveform result{waveform};
    // Already merged on the way in; merging again would be a second pass
    // over every sample for nothing.
    result.setSegments(std::move(segments), false);
    return result;
}

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
        // Filtering defaults ON - a duty analyst wants the filtered trace
        // almost always, and asking for it should not be a thing to
        // remember.  filter=false gives the samples as miniSEED had them.
        //
        // Worth knowing that the two options interact: filtering turns
        // integer counts into fractional doubles, and delta encoding is
        // declined on fractional samples, so filter=true silently makes
        // enableDeltaEncoding a no-op.  Unfiltered counts delta-encode
        // properly.
        const auto applyFilter = readFlag("filter", true);

        // Which channels to draw: the ones the preferred origin was
        // actually located from.  Deliberately naive - it asks for a
        // waveform per picked channel and nothing else, so an event
        // located on eight stations costs eight streams rather than every
        // channel that was running.
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
        if (!event->has_value())
        {
            return ::makeMessageResponse(
                404, "No event " + std::to_string(eventIdentifier));
        }
        if (!(*event)->hasOrigins())
        {
            return ::makeMessageResponse(
                404, "Event " + std::to_string(eventIdentifier)
                   + " has no origin to take waveforms around");
        }
        const auto streamIdentifiers
            = ::streamsFromArrivals((*event)->preferredOrigin());
        if (streamIdentifiers.empty())
        {
            // An origin with no arrivals is a real thing - picks not
            // associated yet - and there is nothing to draw for it.
            return ::makeMessageResponse(
                404, "Event " + std::to_string(eventIdentifier)
                   + " has no picked channels to draw");
        }
        SPDLOG_LOGGER_INFO(context.logger,
                           "Fetching {} stream(s) for event {}",
                           streamIdentifiers.size(), eventIdentifier);

        const auto waveforms
            = context.aqmsDatabase->fetchWaveforms(eventIdentifier,
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
                                    "event {}", eventIdentifier);
                return ::makeMessageResponse(
                    500, "The AQMS waveform query failed - this is a "
                         "backend fault, not a missing waveform");
            }
            SPDLOG_LOGGER_ERROR(context.logger,
                                "Could not fetch waveforms for event {}",
                                eventIdentifier);
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
                           "{} (filter={}, delta={}, quantized={})",
                           waveforms->size(), nSegments,
                           authorization.identity->user,
                           applyFilter,
                           encoding.enableDeltaEncoding,
                           encoding.enableQuantization);
        // Demeaned then bandpass filtered, per segment.  A segment that
        // cannot be filtered goes out as it arrived rather than failing
        // the request - see filterWaveform.
        std::vector<AQMS::Waveform> filtered;
        if (applyFilter)
        {
            filtered.reserve(waveforms->size());
            for (const auto &waveform : *waveforms)
            {
                filtered.push_back(::filterWaveform(waveform,
                                                    context.logger.get()));
            }
        }
        else
        {
            filtered = *waveforms;
        }
        return ::makeDataResponse(
            200,
            "Found " + std::to_string(filtered.size()) + " waveform(s) in "
                     + std::to_string(nSegments) + " segment(s)",
            AQMS::toJSON(filtered, encoding));
    }); 
}
}

#endif

#ifndef METRICS_HPP
#define METRICS_HPP
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <opentelemetry/nostd/shared_ptr.h>
#include <opentelemetry/metrics/meter_provider.h>
#include <opentelemetry/exporters/otlp/otlp_http.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_http_metric_exporter_options.h>
#ifdef WITH_OTLP_GRPC
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_factory.h>
#include <opentelemetry/exporters/otlp/otlp_grpc_metric_exporter_options.h>
#endif
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_factory.h>
#include <opentelemetry/sdk/metrics/export/periodic_exporting_metric_reader_options.h>
#include <opentelemetry/sdk/metrics/meter_context.h>
#include <opentelemetry/sdk/metrics/meter_context_factory.h>
#include <opentelemetry/sdk/metrics/meter_provider_factory.h>
#include <opentelemetry/sdk/metrics/provider.h>
#include <opentelemetry/sdk/metrics/view/instrument_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/meter_selector_factory.h>
#include <opentelemetry/sdk/metrics/view/view_factory.h>
#include "otelOptions.hpp"

namespace
{

bool metricsInitialized{false};

void initializeRouteHistogram(
    opentelemetry::sdk::metrics::MeterProvider *providerInstance)
{
    // Histogram config
    auto histogramInstrumentSelector
        = opentelemetry::sdk::metrics::InstrumentSelectorFactory::Create(
             opentelemetry::sdk::metrics::InstrumentType::kHistogram,
             "route_duration_histogram",
             "s");  
    auto histogramMeterSelector
        = opentelemetry::sdk::metrics::MeterSelectorFactory::Create(
             "route_duration",
             "1.2.0",
             "https://opentelemetry.io/schemas/1.2.0");
    auto histogramAggregationConfig
        = std::make_shared<opentelemetry::sdk::metrics::HistogramAggregationConfig> (); 
    histogramAggregationConfig->boundaries_
        = std::vector<double> {
                               0.00000,
                               0.00001,
                               0.00005,
                               0.00010,
                               0.00050,
                               0.00100,
                               0.00500,
                               0.01000,
                               0.05000,
                               0.10000,
                               0.50000,
                               1.00000,
                               10.0000,
                               100.000,
                               1000.00, // Hopefully user gives up now
                               };
    auto histogramView 
        = opentelemetry::sdk::metrics::ViewFactory::Create(
             "route_duration",
             "Time required to for a route to succcessfully complete",
             opentelemetry::sdk::metrics::AggregationType::kHistogram,
             histogramAggregationConfig);

    providerInstance->AddView(std::move(histogramInstrumentSelector),
                      std::move(histogramMeterSelector),
                      std::move(histogramView));

}

void initializeHTTP(
    const bool exportMetrics,
    const auto &otelHTTPMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpHttpMetricExporterOptions exporterOptions;
    exporterOptions.url = otelHTTPMetricsOptions.url
                        + otelHTTPMetricsOptions.suffix;
    //exporterOptions.console_debug = debug != "" && debug != "0" && debug != "no";
    exporterOptions.content_type
        = otel::exporter::otlp::HttpRequestContentType::kBinary;

    auto exporter
        = otel::exporter::otlp::OtlpHttpMetricExporterFactory::Create(
             exporterOptions);
    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelHTTPMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelHTTPMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    ::initializeRouteHistogram(metricsProvider.get());

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}

#ifdef WITH_OTLP_GRPC
void initializeGRPC(
    const bool exportMetrics,
    const auto &otelGRPCMetricsOptions)
{
    if (!exportMetrics){return;}
    namespace otel = opentelemetry;
    otel::exporter::otlp::OtlpGrpcMetricExporterOptions exporterOptions;
    exporterOptions.endpoint = otelGRPCMetricsOptions.url;
    exporterOptions.use_ssl_credentials = false;
    if (!otelGRPCMetricsOptions.certificatePath.empty())
    {   
        exporterOptions.use_ssl_credentials = true;
        exporterOptions.ssl_credentials_cacert_path
           = otelGRPCMetricsOptions.certificatePath;
    }   
    auto exporter
        = otel::exporter::otlp::OtlpGrpcMetricExporterFactory::Create(
             exporterOptions);

    // Initialize and set the global MeterProvider
    otel::sdk::metrics::PeriodicExportingMetricReaderOptions readerOptions;
    readerOptions.export_interval_millis
        = otelGRPCMetricsOptions.exportInterval;
    readerOptions.export_timeout_millis
        = otelGRPCMetricsOptions.exportTimeOut;

    auto reader
        = otel::sdk::metrics::PeriodicExportingMetricReaderFactory::Create(
             std::move(exporter),
             readerOptions);

    auto context = otel::sdk::metrics::MeterContextFactory::Create();
    context->AddMetricReader(std::move(reader));

    auto metricsProvider
        = otel::sdk::metrics::MeterProviderFactory::Create(
             std::move(context));

    ::initializeRouteHistogram(metricsProvider.get());

    const std::shared_ptr<otel::metrics::MeterProvider>
        provider(std::move(metricsProvider));
    otel::sdk::metrics::Provider::SetMeterProvider(provider);
    metricsInitialized = true;
}
#endif

}

namespace AQMSDutyReviewBackend::Metrics
{

void initialize(
    // NOLINTNEXTLINE(misc-include-cleaner)
    const ProgramOptions &options)
{
    if (options.exportMetricsWithHTTP)
    {   
        return ::initializeHTTP(options.exportMetrics,
                                options.otelHTTPMetricsOptions);
    }   
    else
    {   
#ifdef WITH_OTLP_GRPC
        return ::initializeGRPC(options.exportMetrics,
                                options.otelGRPCMetricsOptions);
#else
        throw std::runtime_error("Recompile with Conan and OTLP_GRPC");
#endif
    }   
}

void cleanup()
{
    if (metricsInitialized)
    {   
        const std::shared_ptr<opentelemetry::metrics::MeterProvider> none;
        opentelemetry::sdk::metrics::Provider::SetMeterProvider(none);
    }   
    metricsInitialized = false;
}

}
#endif

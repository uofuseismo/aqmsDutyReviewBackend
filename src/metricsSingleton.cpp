#include "aqmsDutyReviewBackend/metricsSingleton.hpp"

using namespace AQMSDutyReviewBackend::Metrics;

MetricsSingleton &MetricsSingleton::getInstance()
{
    static MetricsSingleton instance;
    return instance;
}

/// Reset
void MetricsSingleton::resetMetrics() noexcept
{
}

/// Initialize
void AQMSDutyReviewBackend::Metrics::initializeMetricsSingleton()
{
    MetricsSingleton::getInstance();
}


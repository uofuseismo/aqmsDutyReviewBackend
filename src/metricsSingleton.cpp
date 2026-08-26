#include <mutex>
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
    const std::scoped_lock lock{mMutex}; 
    for (auto &counter : mServerErrorCounter)
    {
        counter.second = 0;
    }
    for (auto &counter : mClientErrorCounter)
    {
        counter.second = 0;
    }
    for (auto &counter : mSuccessCounter)
    {
        counter.second = 0;
    }
}

/// Initialize
void AQMSDutyReviewBackend::Metrics::initializeMetricsSingleton()
{
    MetricsSingleton::getInstance();
}


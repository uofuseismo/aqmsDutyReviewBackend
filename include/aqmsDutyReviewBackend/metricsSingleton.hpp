#ifndef AQMS_DUTY_REVIEW_BACKEND_METRICS_SINGLETON_HPP
#define AQMS_DUTY_REVIEW_BACKEND_METRICS_SINGLETON_HPP
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <map>
namespace AQMSDutyReviewBackend::Metrics
{
/// @class MetricsSingleton metricsSingleton.hpp
/// @brief A globally accessible point from which to read and write application
///        metrics.
/// @note This should be instantiated at application startup.
///       @sa \initializeMetricsSingleton().
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class MetricsSingleton
{
public:
    /// @result An instance of the singleton.
    [[maybe_unused]] static MetricsSingleton &getInstance();

    /// @brief Resets the counters an dutilization.  This is useful for unit tests.
    void resetMetrics() noexcept;
private:
    MetricsSingleton() = default;
    ~MetricsSingleton() = default;
    mutable std::mutex mMutex;
    // The following are a (map, count)
    std::map<std::string, int64_t> mServerErrorCounter; // 500 response codes
    std::map<std::string, int64_t> mClientErrorCounter; // 400 response codes
    std::map<std::string, int64_t> mSuccessCounter;     // 200 response codes
};

/// @brief Initializes the metrics singleton once and for all.  This is to be
///        used at application start up.
void initializeMetricsSingleton();
}
#endif

#ifndef AQMS_DUTY_REVIEW_BACKEND_SIGNAL_PROCESSING_HPP
#define AQMS_DUTY_REVIEW_BACKEND_SIGNAL_PROCESSING_HPP
#include <vector>
namespace AQMSDutyReviewBackend::SignalProcessing
{
/// @brief Removes the mean from the input vector.
/// @param[in] x   The signal from which to remove the mean. 
/// @throws std::invalid_argument if x.empty() is true.
[[nodiscard]] std::vector<double> demean(const std::vector<double> &x);
}
#endif

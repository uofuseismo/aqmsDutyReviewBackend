#ifndef AQMS_DUTY_REVIEW_BACKEND_SIGNAL_PROCESSING_BANDPASS_HPP
#define AQMS_DUTY_REVIEW_BACKEND_SIGNAL_PROCESSING_BANDPASS_HPP
#include <vector>
namespace AQMSDutyReviewBackend::SignalProcessing
{
/// @brief Typically the packets sampling rates will be slightly off from a 
///        pre-designed filter.  This attempts to get the appropriate sampling
///        rate that has a bandpass filter design.
/// @param[in] samplingRate   The segment's observed sampling rate in Hz.
/// @throws std::invalid_argument if sampling rate is not positive.
/// @throws std::runtime_error if there exists no design for this sampling rate.
[[nodiscard]] int getValidSamplingRate(double samplingRate);
/// @brief Bandpass filters the input signal using a pre-defined filter.
/// @param[in] x  The signal to filter.
/// @param[in] samplingRate  The sampling rate in Hz.  \c getValidSamplingRate().
/// @result The filtered signal.
[[nodiscard]] std::vector<double> bandpassFilter(const std::vector<double> &x, int samplingRate);
}
#endif

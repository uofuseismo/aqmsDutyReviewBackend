#ifndef AQMS_DUTY_REVIEW_BACKEND_HASH_HPP
#define AQMS_DUTY_REVIEW_BACKEND_HASH_HPP
#include <string>
#include <string_view>

namespace AQMSDutyReviewBackend
{
/// @brief Hashes a payload so a client can tell whether it changed.
/// @param[in] data  The bytes to hash - typically a serialized JSON body.
/// @result The hash as lower-case hex.
/// @note For change detection, not for security: a frontend holding a
///       catalog asks for the hash, compares it with the one it has, and
///       only downloads the body when they differ.
/// @note BLAKE2b via libsodium rather than std::hash.  std::hash is
///       allowed to differ between runs and between implementations, so a
///       client comparing today's hash with yesterday's could be told
///       everything changed because the backend restarted.  This is
///       stable for the same bytes, forever.
[[nodiscard]] std::string hash(std::string_view data);
}
#endif

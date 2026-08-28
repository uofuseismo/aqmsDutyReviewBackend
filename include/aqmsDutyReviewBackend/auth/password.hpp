#ifndef AQMS_DUTY_REVIEW_BACKEND_AUTH_PASSWORD_HPP
#define AQMS_DUTY_REVIEW_BACKEND_AUTH_PASSWORD_HPP
#include <string>

namespace AQMSDutyReviewBackend::Auth
{
/// @brief Hashes a password for storage.
/// @param[in] password  The plain-text password.
/// @result The encoded argon2 hash, safe to put in a TEXT column.
/// @note Plain text never reaches the database - this is what stands
///       between the two.  The cost parameters are fixed here so that
///       hashing and the "does this need re-hashing" check cannot drift
///       apart; when they do, every login pays for a full hash and a
///       write.
/// @throws std::runtime_error if there is not enough memory to hash.
[[nodiscard]] std::string hashPassword(const std::string &password);

/// @brief Generates a throwaway password for a new or reset account.
/// @result A random password.
/// @note Distinct and random every time, on purpose.  A provisional
///       password is a live credential until it is replaced, and a shared
///       "changeme" across a dozen accounts is the failure this whole
///       provisional design otherwise invites: unwatched accounts with a
///       password everybody knows.
/// @note The alphabet omits characters that are read wrongly down a
///       telephone - no O or 0, no l, I, or 1 - because that is how these
///       actually reach the person they belong to.
[[nodiscard]] std::string generateTemporaryPassword();
}
#endif

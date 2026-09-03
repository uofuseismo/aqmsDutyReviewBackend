#ifndef AQMS_DUTY_REVIEW_BACKEND_AUTH_PASSWORD_HPP
#define AQMS_DUTY_REVIEW_BACKEND_AUTH_PASSWORD_HPP
#include <cstddef>
#include <optional>
#include <string>

namespace AQMSDutyReviewBackend::Auth
{
/// @brief What a password must look like to be accepted.
/// @note Policy, not mechanism - so it is configured rather than compiled
///       in, and lives with the other UserManagement settings.  It is
///       enforced here and nowhere else: the database is never shown a
///       plain-text password, so it could not check this even if it
///       wanted to, and a CHECK constraint could not say WHY it refused.
struct PasswordPolicy
{
    /// The shortest acceptable password.  Length is the requirement that
    /// actually buys entropy; the two flags below are here because sites
    /// ask for them, not because they are worth much next to this.
    std::size_t minimumLength{12};
    /// Whether the password must contain a digit.
    bool requiresNumber{false};
    /// Whether the password must contain something neither letter nor
    /// digit.
    bool requiresSpecialCharacter{false};
    /// Whether a password change must actually change the password.
    ///
    /// On by default, unlike the two flags above, and for a different kind
    /// of reason.  A provisional account is issued a password that reaches
    /// its owner by whatever channel was to hand - read down a telephone,
    /// left in a message - and the whole point of making them change it is
    /// that the issued one has been seen by somebody else.  Re-entering it
    /// satisfies the letter of "you must change your password" while
    /// leaving the account exactly as exposed as it was.
    ///
    /// Enforced at the change-password route rather than in
    /// passwordPolicyProblem, which is given a password and no history to
    /// compare it against.
    bool newAndOldPasswordMustBeDifferent{true};
};

/// @brief Checks a password against the policy.
/// @param[in] password  The plain-text password to judge.
/// @param[in] policy    What it is judged against.
/// @result Nullopt if the password is acceptable, otherwise a sentence
///         saying what is wrong with it - ready to hand back as the
///         "message" of a 400.
/// @note Returns the reason rather than a bool because the caller cannot
///       reconstruct one: the point of publishing the policy is that a
///       person can fix their password, and "rejected" does not tell them
///       how.
/// @note Judges only what it is given.  Passwords this application
///       GENERATES do not come through here - see the note on
///       generateTemporaryPassword.
[[nodiscard]] std::optional<std::string> passwordPolicyProblem(
    const std::string &password, const PasswordPolicy &policy);

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
/// @note DELIBERATELY not checked against PasswordPolicy.  The alphabet
///       here is letters and digits only, so a policy requiring a special
///       character would reject every password this ever produced, and
///       one requiring a digit would reject about one in twelve.  That
///       would be the wrong thing to fix: at 16 characters from a
///       56-character alphabet these carry ~93 bits of entropy, which is
///       far past anything a composition rule buys, and they are
///       short-lived by construction.  The policy exists to stop a PERSON
///       choosing "password1"; it has no business judging 93 random bits.
[[nodiscard]] std::string generateTemporaryPassword();
}
#endif

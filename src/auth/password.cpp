#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sodium/crypto_pwhash.h>
#include <sodium/randombytes.h>
#include "aqmsDutyReviewBackend/auth/password.hpp"

namespace
{

/// The argon2 cost parameters.  Used for hashing AND for deciding whether a
/// stored hash needs re-hashing - comparing against different ones makes
/// every login look stale and pays for a full hash and a write each time.
constexpr unsigned long long OPERATIONS_LIMIT{crypto_pwhash_OPSLIMIT_MODERATE};
constexpr std::size_t MEMORY_LIMIT{crypto_pwhash_MEMLIMIT_MODERATE};

/// Deliberately missing O, 0, l, I, and 1 - a provisional password is read
/// out loud or typed from a note more often than it is copied and pasted.
constexpr std::string_view ALPHABET{
    "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnpqrstuvwxyz23456789"};

/// Long enough that the missing characters cost nothing.
constexpr std::size_t PASSWORD_LENGTH{16};

/// @brief std::isalnum without the undefined behaviour.
/// @note The <cctype> predicates take an int that must be representable as
///       unsigned char.  A password is bytes, and on a signed-char
///       platform a UTF-8 continuation byte arrives negative, which is
///       undefined - so the cast is not decoration.
[[nodiscard]] bool isAlphanumeric(const char character) noexcept
{
    return std::isalnum(static_cast<unsigned char> (character)) != 0;
}

[[nodiscard]] bool isDigit(const char character) noexcept
{
    return std::isdigit(static_cast<unsigned char> (character)) != 0;
}

}

std::optional<std::string>
AQMSDutyReviewBackend::Auth::passwordPolicyProblem(
    const std::string &password, const PasswordPolicy &policy)
{
    // Length in bytes, not characters.  The two differ only for non-ASCII
    // input, and they differ in the safe direction: a multi-byte character
    // counts as more than one, so nothing shorter than the minimum is ever
    // accepted.  Counting code points would mean deciding an encoding this
    // layer has no business deciding.
    if (password.size() < policy.minimumLength)
    {
        return "Password must be at least "
             + std::to_string(policy.minimumLength) + " characters";
    }
    if (policy.requiresNumber &&
        std::none_of(password.begin(), password.end(), ::isDigit))
    {
        return "Password must contain a number";
    }
    if (policy.requiresSpecialCharacter &&
        std::all_of(password.begin(), password.end(), ::isAlphanumeric))
    {
        return "Password must contain a special character";
    }
    return std::nullopt;
}

std::string AQMSDutyReviewBackend::Auth::hashPassword(
    const std::string &password)
{
    std::string hashedPassword;
    hashedPassword.resize(crypto_pwhash_STRBYTES);
    if (crypto_pwhash_str(hashedPassword.data(),
                          password.c_str(),
                          password.length(),
                          ::OPERATIONS_LIMIT,
                          ::MEMORY_LIMIT) != 0)
    {
        throw std::runtime_error("Out of memory");
    }
    // crypto_pwhash_str wrote a NUL-terminated C string into a maximum-size
    // buffer; the encoded hash is shorter, so truncate at the first NUL -
    // postgres rejects TEXT with embedded NULs.
    hashedPassword.resize(std::strlen(hashedPassword.c_str()));
    return hashedPassword;
}

std::string AQMSDutyReviewBackend::Auth::generateTemporaryPassword()
{
    std::string password;
    password.reserve(::PASSWORD_LENGTH);
    for (std::size_t i = 0; i < ::PASSWORD_LENGTH; ++i)
    {
        // randombytes_uniform, not a modulo of a random byte: the modulo
        // is biased towards the start of the alphabet whenever 256 is not
        // a multiple of its length, and it is not.
        const auto index
            = randombytes_uniform(static_cast<uint32_t> (::ALPHABET.size()));
        password.push_back(::ALPHABET[index]);
    }
    return password;
}

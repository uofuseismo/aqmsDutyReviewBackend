#include <cstddef>
#include <cstring>
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

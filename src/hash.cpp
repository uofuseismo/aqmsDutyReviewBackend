#include <string>
#include <string_view>
#include <vector>
#include <sodium/crypto_generichash.h>
#include <sodium/utils.h>
#include "aqmsDutyReviewBackend/hash.hpp"

std::string AQMSDutyReviewBackend::hash(const std::string_view data)
{
    std::vector<unsigned char> digest(crypto_generichash_BYTES);
    // Unkeyed, so the same bytes always give the same digest - which is
    // the whole point when a client is comparing against one it cached
    // days ago.
    crypto_generichash(digest.data(), digest.size(),
                       reinterpret_cast<const unsigned char *> (data.data()),
                       data.size(),
                       nullptr, 0);
    std::string hexadecimal;
    hexadecimal.resize(digest.size()*2 + 1);
    sodium_bin2hex(hexadecimal.data(), hexadecimal.size(),
                   digest.data(), digest.size());
    // sodium_bin2hex NUL-terminates; drop the terminator.
    hexadecimal.resize(digest.size()*2);
    return hexadecimal;
}

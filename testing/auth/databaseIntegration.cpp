/// Purpose: Exercises the Database authenticator against a real
/// PostgreSQL database built by scripts/database.
///
/// These tests are HIDDEN by default - the tag starts with a dot, so
/// Catch2 skips them unless they are asked for by name or tag.  They need
/// a live database and would otherwise fail on any machine that has not
/// built one.  Run them deliberately:
///
///     AQMSDB_TEST_DATABASE=aqmsdb_cxxcheck \
///     AQMSDB_TEST_USER=cxxcheck_writer \
///     AQMSDB_TEST_PASSWORD=... \
///     ./unitTests "[.database]"
///
/// The database must have an activated administrator named 'root'; that
/// is a superuser bootstrap step, exactly as it is in a real deployment:
///
///     sudo -u postgres psql -d <db> \
///       -c "SELECT add_user('root', 'placeholder-hash', 'admin');"
///
/// What these cover that the unit tests cannot: that the SQL this class
/// writes actually names functions that exist, with the argument counts
/// they really have, and that the backend role is actually granted them.
/// A signature mismatch or a missing grant is invisible until something
/// connects.
#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <sodium.h>
#include <catch2/catch_test_macros.hpp>
#include "aqmsDutyReviewBackend/auth/database.hpp"
#include "aqmsDutyReviewBackend/auth/databaseOptions.hpp"
#include "aqmsDutyReviewBackend/auth/authenticator.hpp"
#include "aqmsDutyReviewBackend/database/credentials.hpp"

using namespace AQMSDutyReviewBackend::Auth;
namespace DB = AQMSDutyReviewBackend::Database;

namespace
{

/// @brief Reads an environment variable, falling back to a default.
std::string environmentOr(const char *name, const std::string &fallback)
{
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const auto *value = std::getenv(name);
    if (value == nullptr){return fallback;}
    const std::string result{value};
    return result.empty() ? fallback : result;
}

/// @brief Builds the authenticator from the environment.
std::unique_ptr<Database> makeDatabase()
{
    DB::Credentials credentials;
    credentials.setUser(::environmentOr("AQMSDB_TEST_USER",
                                        "cxxcheck_writer"));
    credentials.setPassword(::environmentOr("AQMSDB_TEST_PASSWORD",
                                            "cxxcheck_rw_pw"));
    credentials.setDatabaseName(::environmentOr("AQMSDB_TEST_DATABASE",
                                                "aqmsdb_cxxcheck"));
    credentials.setHost(::environmentOr("AQMSDB_TEST_HOST", "localhost"));
    DatabaseOptions options;
    options.setCredentials(credentials);
    return std::make_unique<Database> (options, nullptr);
}

/// @brief base64 (original alphabet, padded), matching what the schema
///        stores and what Database expects.
std::string toBase64(const std::vector<unsigned char> &bytes)
{
    std::string encoded;
    encoded.resize(sodium_base64_ENCODED_LEN(
                       bytes.size(), sodium_base64_VARIANT_ORIGINAL));
    sodium_bin2base64(encoded.data(), encoded.size(),
                      bytes.data(), bytes.size(),
                      sodium_base64_VARIANT_ORIGINAL);
    // sodium_bin2base64 NUL-terminates; drop the terminator.
    encoded.resize(encoded.find('\0') == std::string::npos
                       ? encoded.size() : encoded.find('\0'));
    return encoded;
}

/// The bootstrap administrator; see the header comment.
constexpr const char *ADMIN{"root"};

}

TEST_CASE("Auth::Database connects and reads permissions", "[.database]")
{
    const auto database = makeDatabase();

    // The bootstrap administrator has to be there, or nothing below can
    // act.
    REQUIRE(database->getPermissions(::ADMIN)
            == IAuthenticator::Permissions::Administrator);
    // An unknown user grants nothing rather than throwing.
    REQUIRE(database->getPermissions("no-such-person-here")
            == IAuthenticator::Permissions::None);
    REQUIRE_THROWS_AS(database->getPermissions(""), std::invalid_argument);
}

TEST_CASE("Auth::Database provisional user lifecycle", "[.database]")
{
    auto database = makeDatabase();
    const std::string user{"cxxcheck_tim"};
    // Leave no wreckage from a previous run.
    static_cast<void> (database->removeUser(::ADMIN, user));

    // Hired: a dummy password valid for an hour, read-only while training.
    REQUIRE(database->addProvisionalUser(
                ::ADMIN, {user, "dummy-password"},
                std::chrono::hours {1},
                IAuthenticator::Permissions::ReadOnly)
            == Database::AdminResult::Succeeded);

    // Still holding the password they were issued.
    const auto provisional = database->mustChangePassword(user);
    REQUIRE(provisional.has_value());
    //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    REQUIRE(*provisional);

    // The dummy password has to work, or they cannot get in to replace it.
    REQUIRE(database->authenticateBasic({user, "dummy-password"})
            == IAuthenticator::Result::Authenticated);
    REQUIRE(database->authenticateBasic({user, "not-the-password"})
            == IAuthenticator::Result::InvalidCredentials);

    // Changing the password IS the activation - there is no second call.
    REQUIRE(database->updatePassword({user, "a-real-password"}));
    const auto activated = database->mustChangePassword(user);
    REQUIRE(activated.has_value());
    //NOLINTNEXTLINE(bugprone-unchecked-optional-access)
    REQUIRE_FALSE(*activated);
    REQUIRE(database->authenticateBasic({user, "a-real-password"})
            == IAuthenticator::Result::Authenticated);
    REQUIRE(database->authenticateBasic({user, "dummy-password"})
            == IAuthenticator::Result::InvalidCredentials);

    // Training over.
    REQUIRE(database->setUserPermission(::ADMIN, user,
                                        IAuthenticator::Permissions::ReadWrite)
            == Database::AdminResult::Succeeded);
    REQUIRE(database->getPermissions(user)
            == IAuthenticator::Permissions::ReadWrite);

    // An unknown user is not provisional, it is absent.
    REQUIRE(database->mustChangePassword("no-such-person-here")
            == std::nullopt);

    REQUIRE(database->removeUser(::ADMIN, user)
            == Database::AdminResult::Succeeded);
    // Removing someone already gone is a failure, not an authorization
    // problem.
    REQUIRE(database->removeUser(::ADMIN, user)
            == Database::AdminResult::Failed);
}

TEST_CASE("Auth::Database refuses an actor who may not act", "[.database]")
{
    auto database = makeDatabase();
    const std::string user{"cxxcheck_alice"};
    static_cast<void> (database->removeUser(::ADMIN, user));

    REQUIRE(database->addUser(::ADMIN, {user, "alice-password"},
                              IAuthenticator::Permissions::ReadWrite)
            == Database::AdminResult::Succeeded);

    // A read_write user is not an administrator.  This must come back as
    // NotAuthorized and not as Failed: the database raises
    // insufficient_privilege rather than returning FALSE precisely so the
    // frontend can tell a 403 from a 400.
    REQUIRE(database->addUser(user, {"cxxcheck_sneaky", "hash"},
                              IAuthenticator::Permissions::ReadOnly)
            == Database::AdminResult::NotAuthorized);
    REQUIRE(database->setUserPermission(user, user,
                                        IAuthenticator::Permissions::Administrator)
            == Database::AdminResult::NotAuthorized);
    REQUIRE(database->removeUser(user, ::ADMIN)
            == Database::AdminResult::NotAuthorized);

    // An actor who does not exist is not an administrator either.
    REQUIRE(database->removeUser("nobody-at-all", user)
            == Database::AdminResult::NotAuthorized);

    // The last administrator cannot be demoted into a lockout.
    REQUIRE(database->setUserPermission(::ADMIN, ::ADMIN,
                                        IAuthenticator::Permissions::ReadWrite)
            == Database::AdminResult::NotAuthorized);

    REQUIRE(database->removeUser(::ADMIN, user)
            == Database::AdminResult::Succeeded);
}

TEST_CASE("Auth::Database rejects a level the schema cannot store",
          "[.database]")
{
    auto database = makeDatabase();
    // None has no counterpart in the CHECK constraint, so it is refused
    // here rather than tripping the constraint and coming back as an
    // indistinguishable "that did not work".
    REQUIRE_THROWS_AS(
        database->addUser(::ADMIN, {"cxxcheck_nobody", "password"},
                          IAuthenticator::Permissions::None),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        database->setUserPermission(::ADMIN, ::ADMIN,
                                    IAuthenticator::Permissions::None),
        std::invalid_argument);
}

TEST_CASE("Auth::Database public key authentication", "[.database]")
{
    auto database = makeDatabase();
    const std::string user{"cxxcheck_carol"};
    static_cast<void> (database->removeUser(::ADMIN, user));
    REQUIRE(database->addUser(::ADMIN, {user, "carol-password"},
                              IAuthenticator::Permissions::ReadOnly)
            == Database::AdminResult::Succeeded);

    // A real ed25519 pair, so the signature genuinely verifies rather
    // than the test asserting that a wrong one fails.
    std::vector<unsigned char> publicKey(crypto_sign_PUBLICKEYBYTES);
    std::vector<unsigned char> secretKey(crypto_sign_SECRETKEYBYTES);
    REQUIRE(crypto_sign_keypair(publicKey.data(), secretKey.data()) == 0);
    const auto encodedPublicKey = toBase64(publicKey);

    REQUIRE(database->addUserKey(user, "laptop", encodedPublicKey));
    // The same key name twice, and the same key for anyone else, are both
    // refused.
    REQUIRE_FALSE(database->addUserKey(user, "laptop", encodedPublicKey));

    const std::string message{"a message the client signed"};
    std::vector<unsigned char> signature(crypto_sign_BYTES);
    unsigned long long signatureLength{0};
    REQUIRE(crypto_sign_detached(
                signature.data(), &signatureLength,
                reinterpret_cast<const unsigned char *> (message.data()),
                message.size(), secretKey.data()) == 0);
    const auto encodedSignature = toBase64(signature);

    REQUIRE(database->authenticateKey(message, encodedSignature,
                                      encodedPublicKey)
            == IAuthenticator::Result::Authenticated);
    // A signature over different bytes must not pass.
    REQUIRE(database->authenticateKey("a different message",
                                      encodedSignature, encodedPublicKey)
            == IAuthenticator::Result::InvalidCredentials);

    // Revoked, not deleted - and a revoked key stops authenticating.
    REQUIRE(database->revokeUserKey(user, "laptop"));
    REQUIRE(database->authenticateKey(message, encodedSignature,
                                      encodedPublicKey)
            == IAuthenticator::Result::InvalidCredentials);
    REQUIRE_FALSE(database->revokeUserKey(user, "laptop"));

    // Junk never reaches the database.
    REQUIRE_THROWS_AS(database->addUserKey(user, "bad", "not-base64!!"),
                      std::invalid_argument);

    REQUIRE(database->removeUser(::ADMIN, user)
            == Database::AdminResult::Succeeded);
}

TEST_CASE("Auth::Database sweeps expired provisional accounts", "[.database]")
{
    auto database = makeDatabase();
    const std::string user{"cxxcheck_frank"};
    static_cast<void> (database->removeUser(::ADMIN, user));

    REQUIRE(database->addProvisionalUser(::ADMIN, {user, "dummy"},
                                         std::chrono::seconds {1},
                                         IAuthenticator::Permissions::ReadOnly)
            == Database::AdminResult::Succeeded);
    std::this_thread::sleep_for(std::chrono::milliseconds {1500});

    // The deadline binds on its own: expired accounts cannot log in
    // whether or not the sweep has reached them.  Without that the sweep
    // interval quietly becomes the real deadline.
    REQUIRE(database->authenticateBasic({user, "dummy"})
            == IAuthenticator::Result::InvalidCredentials);

    REQUIRE(database->deleteExpiredProvisionalUsers() >= 1);
    REQUIRE(database->getPermissions(user)
            == IAuthenticator::Permissions::None);
}

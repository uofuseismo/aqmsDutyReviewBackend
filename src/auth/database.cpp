#include <chrono>
#include <cstring>
#include <exception>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include <sodium/crypto_pwhash.h>
#include <sodium/crypto_sign.h>
#include <sodium/utils.h>
#include "aqmsDutyReviewBackend/auth/database.hpp"
#include "aqmsDutyReviewBackend/auth/databaseOptions.hpp"
#include "aqmsDutyReviewBackend/auth/authenticator.hpp"
#include "aqmsDutyReviewBackend/auth/password.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"
#include "aqmsDutyReviewBackend/database/credentials.hpp"
#include "aqmsDutyReviewBackend/database/drp/userStore.hpp"

using namespace AQMSDutyReviewBackend::Auth;
namespace DRP = AQMSDutyReviewBackend::Database::DRP;

namespace
{

/// @brief Renders a permissions level for the users.permission column.
/// @throws std::invalid_argument if the level is None - the database's CHECK
///         constraint allows only read_only, read_write, and admin, so None
///         has nothing to be stored as.  Catching it here reports the real
///         problem rather than letting the CHECK fail and surface as an
///         indistinguishable "that did not work".
[[nodiscard]] std::string toStorablePermission(
    const IAuthenticator::Permissions permissions)
{
    if (permissions == IAuthenticator::Permissions::None)
    {
        throw std::invalid_argument(
            "Permissions cannot be None - the database stores read_only, "
            "read_write, or admin");
    }
    return IAuthenticator::permissionsToString(permissions);
}

/// @brief Decodes base64 text into exactly expectedLength bytes.
[[nodiscard]] std::optional<std::vector<unsigned char>>
    base64Decode(const std::string &text, const std::size_t expectedLength)
{
    std::vector<unsigned char> result(expectedLength);
    std::size_t actualLength{0};
    if (sodium_base642bin(result.data(), result.size(),
                          text.data(), text.size(),
                          nullptr, &actualLength, nullptr,
                          sodium_base64_VARIANT_ORIGINAL) != 0)
    {
        return std::nullopt;
    }
    if (actualLength != expectedLength){return std::nullopt;}
    return result;
}

}

class Database::DatabaseImpl
{
public:
    DatabaseImpl(std::shared_ptr<DRP::UserStore> users,
                 std::shared_ptr<spdlog::logger> logger) :
        mUsers(std::move(users)),
        mLogger(std::move(logger))
    {
        if (mUsers == nullptr)
        {
            throw std::invalid_argument("User store is null");
        }
        if (mLogger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
            constexpr const char *loggerName{"AuthDatabaseConsole"};
            mLogger = spdlog::get(loggerName);
            if (mLogger == nullptr)
            {
                mLogger = spdlog::stdout_color_mt(loggerName);
            }
            // NOLINTEND(misc-include-cleaner)
        }
    }

    /// @brief Verifies a request signed with a registered key.
    [[nodiscard]] IAuthenticator::Result authenticateByKey(
        const std::string &message,
        const std::string &signature,
        const std::string &publicKey)
    {
        // Is this an active key we know?
        const auto user = mUsers->getUserByKey(publicKey);
        if (!user)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "User not found for provided public key");
            return IAuthenticator::Result::InvalidCredentials;
        }
        const auto publicKeyBytes
            = ::base64Decode(publicKey, crypto_sign_PUBLICKEYBYTES);
        const auto signatureBytes
            = ::base64Decode(signature, crypto_sign_BYTES);
        if (!publicKeyBytes || !signatureBytes)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Malformed signature or public key for {}",
                               *user);
            return IAuthenticator::Result::InvalidCredentials;
        }
        // Did the holder of the private key sign this message?
        if (crypto_sign_verify_detached(
                signatureBytes->data(),
                reinterpret_cast<const unsigned char *> (message.data()),
                message.size(),
                publicKeyBytes->data()) != 0)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Signature verification failed for {}",
                               *user);
            return IAuthenticator::Result::InvalidCredentials;
        }
        if (!mUsers->recordKeyUse(publicKey))
        {
            SPDLOG_LOGGER_WARN(mLogger, "Failed to record key use for {}",
                               *user);
        }
        SPDLOG_LOGGER_INFO(mLogger, "Verified {} by key", *user);
        return IAuthenticator::Result::Authenticated;
    }

    /// @brief Verifies a user name and password.
    [[nodiscard]] IAuthenticator::Result authenticateByPassword(
        const std::pair<std::string, std::string> &userAndPassword)
    {
        const auto &[user, password] = userAndPassword;
        const auto hashedPassword = mUsers->getPasswordHash(user);
        if (hashedPassword == std::nullopt)
        {
            // No such user, or a provisional account whose deadline has
            // passed.  Both are a rejection and the client is told which in
            // neither case - the difference is what lets someone enumerate
            // valid user names.
            SPDLOG_LOGGER_WARN(mLogger, "{} not in database", user);
            return IAuthenticator::Result::InvalidCredentials;
        }
        if (crypto_pwhash_str_verify(hashedPassword->c_str(),
                                     password.c_str(),
                                     password.length()) != 0)
        {
            SPDLOG_LOGGER_WARN(mLogger, "{} provided incorrect password",
                               user);
            return IAuthenticator::Result::InvalidCredentials;
        }
        if (!mUsers->recordLogin(user))
        {
            SPDLOG_LOGGER_WARN(mLogger, "Failed to record login for {}",
                               user);
        }
        // If the cost parameters have moved on since this hash was made,
        // re-hash at the current ones.
        if (crypto_pwhash_str_needs_rehash(hashedPassword->c_str(),
                                           crypto_pwhash_OPSLIMIT_MODERATE,
                                           crypto_pwhash_MEMLIMIT_MODERATE)
            != 0)
        {
            SPDLOG_LOGGER_INFO(mLogger, "Rehashing password for {}", user);
            try
            {
                if (!mUsers->updatePassword(user, AQMSDutyReviewBackend::Auth::hashPassword(password)))
                {
                    SPDLOG_LOGGER_WARN(mLogger,
                                       "Password rehash did not update {}",
                                       user);
                }
            }
            catch (const std::exception &e)
            {
                // Not fatal - the user is authenticated either way and the
                // rehash can happen on their next login.
                SPDLOG_LOGGER_WARN(mLogger,
                                   "Password update failed because {}",
                                   std::string {e.what()});
            }
        }
        return IAuthenticator::Result::Authenticated;
    }

    std::shared_ptr<DRP::UserStore> mUsers;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
};

/// Constructor - injected
Database::Database(std::shared_ptr<DRP::UserStore> users,
                   std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<DatabaseImpl> (std::move(users), std::move(logger)))
{
}

/// Constructor - convenience, builds its own client and store
Database::Database(const DatabaseOptions &options,
                   std::shared_ptr<spdlog::logger> logger) :
    Database(std::make_shared<DRP::UserStore>
             (std::make_shared<AQMSDutyReviewBackend::Database::Client>
              (options.getCredentials(), logger),
              logger),
             logger)
{
}

/// Auth the user
IAuthenticator::Result Database::authenticateBasic(
    const std::pair<std::string, std::string> &userNameAndPassword)
{
    if (userNameAndPassword.first.empty())
    {
        throw std::invalid_argument("User is empty");
    }
    // A database problem is our problem, not the client's - so this is a
    // server error rather than an escaped exception.
    try
    {
        return pImpl->authenticateByPassword(userNameAndPassword);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Basic authentication failed because {}",
                            std::string {e.what()});
        return IAuthenticator::Result::ServerError;
    }
}

/// Permissions
IAuthenticator::Permissions
Database::getPermissions(const std::string &user) const
{
    const auto permission = pImpl->mUsers->getPermission(user);
    // No such user grants nothing.
    if (permission == std::nullopt)
    {
        return IAuthenticator::Permissions::None;
    }
    return IAuthenticator::stringToPermissions(*permission);
}

/// Still holding the password they were issued?
std::optional<bool>
Database::mustChangePassword(const std::string &user) const
{
    return pImpl->mUsers->mustChangePassword(user);
}

/// Add a user
Database::AdminResult Database::addUser(
    const std::string &actor,
    const std::pair<std::string, std::string> &userNameAndPassword,
    const IAuthenticator::Permissions permissions)
{
    const auto &[user, password] = userNameAndPassword;
    if (password.empty()){throw std::invalid_argument("Password is empty");}
    return pImpl->mUsers->addUser(actor, user, AQMSDutyReviewBackend::Auth::hashPassword(password),
                                  ::toStorablePermission(permissions));
}

/// Add a provisional user
Database::AdminResult Database::addProvisionalUser(
    const std::string &actor,
    const std::pair<std::string, std::string> &userNameAndPassword,
    const std::chrono::seconds &validFor,
    const IAuthenticator::Permissions permissions)
{
    const auto &[user, password] = userNameAndPassword;
    if (password.empty()){throw std::invalid_argument("Password is empty");}
    return pImpl->mUsers->addProvisionalUser(
        actor, user, AQMSDutyReviewBackend::Auth::hashPassword(password), validFor,
        ::toStorablePermission(permissions));
}

/// Change someone's level
Database::AdminResult Database::setUserPermission(
    const std::string &actor,
    const std::string &user,
    const IAuthenticator::Permissions permissions)
{
    return pImpl->mUsers->setUserPermission(
        actor, user, ::toStorablePermission(permissions));
}

/// Reset a forgotten password
Database::AdminResult Database::resetUserPassword(
    const std::string &actor,
    const std::pair<std::string, std::string> &userNameAndPassword,
    const std::chrono::seconds &validFor)
{
    const auto &[user, password] = userNameAndPassword;
    if (password.empty()){throw std::invalid_argument("Password is empty");}
    return pImpl->mUsers->resetUserPassword(actor, user,
                                            AQMSDutyReviewBackend::Auth::hashPassword(password),
                                            validFor);
}

/// Remove a user
Database::AdminResult Database::removeUser(const std::string &actor,
                                           const std::string &user)
{
    return pImpl->mUsers->removeUser(actor, user);
}

/// Update a password
bool Database::updatePassword(
    const std::pair<std::string, std::string> &userNameAndPassword)
{
    const auto &[user, password] = userNameAndPassword;
    if (user.empty()){throw std::invalid_argument("User is empty");}
    if (password.empty()){throw std::invalid_argument("Password is empty");}
    return pImpl->mUsers->updatePassword(user, AQMSDutyReviewBackend::Auth::hashPassword(password));
}

/// Sweep the expired provisional accounts
int Database::deleteExpiredProvisionalUsers()
{
    return pImpl->mUsers->deleteExpiredProvisionalUsers();
}

/// Register a key
bool Database::addUserKey(const std::string &user,
                          const std::string &keyName,
                          const std::string &publicKey)
{
    // Fail fast on junk before it lands in the database.  The store holds
    // text; deciding that the text IS an ed25519 key is a crypto question
    // and so belongs here.
    if (!publicKey.empty() &&
        !::base64Decode(publicKey, crypto_sign_PUBLICKEYBYTES))
    {
        throw std::invalid_argument(
            "Public key is not a base64 ed25519 public key");
    }
    return pImpl->mUsers->addUserKey(user, keyName, publicKey);
}

/// Revoke a key
bool Database::revokeUserKey(const std::string &user,
                             const std::string &keyName)
{
    return pImpl->mUsers->revokeUserKey(user, keyName);
}

/// Auth by key
IAuthenticator::Result Database::authenticateKey(
    const std::string &message,
    const std::string &signature,
    const std::string &publicKey)
{
    if (message.empty()){throw std::invalid_argument("Message is empty");}
    if (signature.empty()){throw std::invalid_argument("Signature is empty");}
    if (publicKey.empty())
    {
        throw std::invalid_argument("Public key is empty");
    }
    // As with basic authentication a database problem is our problem.
    try
    {
        return pImpl->authenticateByKey(message, signature, publicKey);
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_ERROR(pImpl->mLogger,
                            "Key authentication failed because {}",
                            std::string {e.what()});
        return IAuthenticator::Result::ServerError;
    }
}

/// Destructor
Database::~Database() = default;

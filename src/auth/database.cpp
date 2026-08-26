#include <chrono>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include <sodium/crypto_pwhash.h>
#include <sodium/crypto_sign.h>
#include <sodium/utils.h>
#include <pqxx/pqxx>
#include "aqmsDutyReviewBackend/auth/database.hpp"
#include "aqmsDutyReviewBackend/auth/databaseOptions.hpp"
#include "aqmsDutyReviewBackend/auth/authenticator.hpp"
#include "aqmsDutyReviewBackend/database/credentials.hpp"

using namespace AQMSDutyReviewBackend::Auth;

namespace
{
/// The SQLSTATE the database raises when the acting user may not do what
/// they asked.  It is deliberately not a FALSE return: a boolean cannot
/// separate "you may not" from "that did not work", and those are a 403
/// and a 400 to the frontend.
constexpr std::string_view INSUFFICIENT_PRIVILEGE{"42501"};

/// @brief Renders a permissions level for the users.permission column.
/// @throws std::invalid_argument if the level is None - the database's
///         CHECK constraint allows only read_only, read_write, and admin,
///         so None has nothing to be stored as.  Catching it here reports
///         the real problem rather than letting the CHECK fail and
///         surface as an indistinguishable "that did not work".
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
}

class Database::DatabaseImpl
{
public:
    DatabaseImpl(const DatabaseOptions &options,
                 std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger))
    {
        if (!mOptions.hasCredentials())
        {
            throw std::invalid_argument("Database credentials not set");
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
        connect();
        mInitialized = isConnected();
    }

    /// @brief Connects to the database.  The caller must hold the mutex.
    /// @note mConnection is mutable so a const read path can re-dial a
    ///       dropped connection; the observable state is unchanged, which
    ///       is what constness is about here.
    void connectUnlocked() const
    {
        disconnectUnlocked();
        const auto credentials = mOptions.getCredentials();
        const auto connectionString = credentials.getConnectionString();
        const auto schema = credentials.getSchema();
        mConnection = std::make_unique<pqxx::connection> (connectionString);
        if (!mConnection || !mConnection->is_open())
        {
            throw std::runtime_error(
                "Failed to establish database connection to "
              + credentials.getDatabaseName()
              + " at " + credentials.getHost());
        }
        // Everything this class touches lives in 'public', so a schema is
        // not normally set.  It stays honoured for a deployment that puts
        // the tables somewhere else.  N.B. this has to be re-issued on
        // every reconnect - a search path is per-session state and a new
        // connection does not inherit the old one's.
        if (schema != std::nullopt)
        {
            SPDLOG_LOGGER_DEBUG(mLogger, "Updating search path to {}",
                                *schema);
            const std::string query = "SET search_path TO "
                                    + mConnection->quote_name(*schema)
                                    + ",public";
            pqxx::work transaction(*mConnection);
            transaction.exec(query);
            transaction.commit();
        }
        SPDLOG_LOGGER_INFO(mLogger,
                           "Connected to {} at {}",
                           credentials.getDatabaseName(),
                           credentials.getHost());
    }

    /// @brief Disconnects.  The caller must hold the mutex.
    void disconnectUnlocked() const noexcept
    {
        if (mConnection)
        {
            try
            {
                mConnection->close();
            }
            catch (...)
            {
                // Closing a connection that is already gone is not a
                // failure worth propagating - we are discarding it either
                // way.
            }
            mConnection = nullptr;
        }
    }

    /// @brief Makes sure there is a usable connection, re-dialling if the
    ///        old one has gone.  The caller must hold the mutex.
    /// @note is_open() only reports what the client believes: a connection
    ///       dropped silently by a server restart or a firewall idle
    ///       timeout can still look open until something is sent down it.
    ///       So this is a cheap filter for the case we can see, not the
    ///       whole answer - the retry in \c execute is what catches the
    ///       rest.
    void ensureConnectedUnlocked() const
    {
        if (mConnection && mConnection->is_open()){return;}
        SPDLOG_LOGGER_WARN(mLogger,
                           "Database connection is down; reconnecting");
        connectUnlocked();
    }

    /// @brief Connect to the database.
    void connect()
    {
        const std::scoped_lock lock(mDatabaseMutex);
        connectUnlocked();
    }

    /// @brief Disconnect from the database.
    void disconnect() noexcept
    {
        const std::scoped_lock lock(mDatabaseMutex);
        disconnectUnlocked();
    }

    /// @brief Am I connected to the database?
    /// @return True indicates the database is connected.
    [[nodiscard]] bool isConnected() const noexcept
    {
        const std::scoped_lock lock(mDatabaseMutex);
        return mConnection && mConnection->is_open();
    }

    /// @brief Runs an operation against the database, re-dialling a
    ///        connection that has dropped.
    /// @note A long-running server will lose its connection sooner or
    ///       later - the database gets restarted, a firewall reaps an idle
    ///       socket - and that must not turn into an authentication
    ///       outage that lasts until someone notices.  So the connection
    ///       is checked before the attempt and re-dialled once if the
    ///       attempt itself finds it dead.
    ///
    ///       Retrying is only safe because libpqxx separates the two ways
    ///       a connection can die.  broken_connection means the
    ///       transaction demonstrably did not complete, so repeating it
    ///       cannot apply it twice.  in_doubt_error means the connection
    ///       died while finishing the transaction and there is no way to
    ///       tell whether the server committed - that one is never
    ///       retried, because a repeat could apply a change twice, and it
    ///       is not this class's call to make silently.
    template<typename F>
    [[nodiscard]] auto execute(F &&operation,
                               const std::string_view what) const
    {
        const std::scoped_lock lock(mDatabaseMutex);
        ensureConnectedUnlocked();
        try
        {
            return operation(*mConnection);
        }
        catch (const pqxx::in_doubt_error &e)
        {
            SPDLOG_LOGGER_ERROR(
                mLogger,
                "Connection lost while committing {} - whether it took "
                "effect is unknown, so it will not be retried: {}",
                what, std::string {e.what()});
            throw;
        }
        catch (const pqxx::broken_connection &e)
        {
            SPDLOG_LOGGER_WARN(
                mLogger,
                "Database connection dropped during {} ({}); reconnecting "
                "and retrying once",
                what, std::string {e.what()});
            // If this throws, it propagates: one retry, never a loop, so
            // a database that is simply down fails fast instead of
            // hanging the request.
            connectUnlocked();
            return operation(*mConnection);
        }
    }

    /// @brief Runs a query returning one row and hands back that row's
    ///        first column.
    /// @note Every auth function in the schema is a scalar SELECT, so this
    ///       is the shape of nearly every call.  Keeping the lock, the
    ///       transaction, and the commit in one place is what stops a new
    ///       call site forgetting the commit and quietly rolling its own
    ///       write back on the transaction's destruction.
    template<typename T>
    [[nodiscard]] T executeScalar(const std::string_view query,
                                  const pqxx::params &parameters) const
    {
        return execute(
            [&](pqxx::connection &connection) -> T
            {
                pqxx::work transaction(connection);
                auto row = transaction.exec(query, parameters).one_row();
                auto result = row.at(0).as<T> ();
                transaction.commit();
                return result;
            },
            query);
    }

    /// @brief As executeScalar, for a column that may come back NULL.
    template<typename T>
    [[nodiscard]] std::optional<T> executeOptionalScalar(
        const std::string_view query,
        const pqxx::params &parameters) const
    {
        return execute(
            [&](pqxx::connection &connection) -> std::optional<T>
            {
                pqxx::work transaction(connection);
                auto row = transaction.exec(query, parameters).one_row();
                std::optional<T> result;
                if (!row.at(0).is_null())
                {
                    result = std::make_optional<T> (row.at(0).as<T> ());
                }
                transaction.commit();
                return result;
            },
            query);
    }

    /// @brief Runs an administrative function and separates "you may not"
    ///        from "that did not work".
    [[nodiscard]] Database::AdminResult executeAdmin(
        const std::string_view query,
        const pqxx::params &parameters,
        const std::string_view what)
    {
        try
        {
            return executeScalar<bool> (query, parameters)
                 ? Database::AdminResult::Succeeded
                 : Database::AdminResult::Failed;
        }
        catch (const pqxx::sql_error &e)
        {
            if (e.sqlstate() == ::INSUFFICIENT_PRIVILEGE)
            {
                SPDLOG_LOGGER_WARN(mLogger, "Refused {}: {}",
                                   what, std::string {e.what()});
                return Database::AdminResult::NotAuthorized;
            }
            throw;
        }
    }

    /// @brief Hashes a password for storage.
    [[nodiscard]] std::string hashPassword(const std::string &password) const
    {
        std::string hashedPassword;
        hashedPassword.resize(crypto_pwhash_STRBYTES);
        if (crypto_pwhash_str(hashedPassword.data(), // Output
                              password.c_str(),      // Input
                              password.length(),
                              mOperationsLimit,
                              mMemoryLimit) != 0)
        {
            throw std::runtime_error("Out of memory");
        }
        // crypto_pwhash_str wrote a NUL-terminated C string into a
        // maximum-size buffer; the encoded hash is shorter than
        // crypto_pwhash_STRBYTES so truncate at the first NUL - postgres
        // rejects TEXT with embedded NULs.
        hashedPassword.resize(std::strlen(hashedPassword.c_str()));
        return hashedPassword;
    }

    /// @brief Updates the user password.  Clearing the provisioning
    ///        deadline is what activates a provisioned account, so this is
    ///        the activation step as well as the change-password one.
    /// @result True on success; false if the user does not exist or is
    ///         already past their deadline.
    [[nodiscard]] bool updatePassword(const std::pair<std::string, std::string> &userAndPassword)
    {
        const auto &user = userAndPassword.first;
        if (user.empty()){throw std::invalid_argument("User is empty");}
        const auto &password = userAndPassword.second;
        if (password.empty()){throw std::invalid_argument("Password is empty");}
        auto hashedPassword = hashPassword(password);
        constexpr std::string_view query{"SELECT update_user_password($1, $2)"};
        return executeScalar<bool> (query, pqxx::params{user, hashedPassword});
    }

    /// @brief Gets the user's permissions level.
    [[nodiscard]] IAuthenticator::Permissions
        getPermissions(const std::string &user) const
    {
        if (user.empty()){throw std::invalid_argument("User is empty");}
        constexpr std::string_view query{"SELECT get_user_permission($1)"};
        // NULL means there is no such user, which grants nothing.
        const auto permission
            = executeOptionalScalar<std::string> (query, pqxx::params{user});
        if (permission == std::nullopt)
        {
            return IAuthenticator::Permissions::None;
        }
        return IAuthenticator::stringToPermissions(*permission);
    }

    /// @brief Is this account still holding the password it was issued?
    [[nodiscard]] std::optional<bool>
        mustChangePassword(const std::string &user) const
    {
        if (user.empty()){throw std::invalid_argument("User is empty");}
        constexpr std::string_view query{
            "SELECT user_must_change_password($1)"};
        return executeOptionalScalar<bool> (query, pqxx::params{user});
    }

    /// @brief Gets the hashed password from the database.
    /// @result The hashed password (or null if the user does not exist,
    ///         or is provisional and past their deadline).
    [[nodiscard]] std::optional<std::string>
        getHashedPasswordFromDatabase(const std::string &user) const
    {
        if (user.empty()){throw std::invalid_argument("User is empty");}
        constexpr std::string_view query{"SELECT get_password_hash($1)"};
        return executeOptionalScalar<std::string> (query,
                                                   pqxx::params{user});
    }

    /// @brief Record a successful login.
    [[nodiscard]] bool recordLogin(const std::string &user)
    {
        constexpr std::string_view query{"SELECT record_login($1)"};
        return executeScalar<bool> (query, pqxx::params{user});
    }

    /// @brief Decodes base64 text into exactly expectedLength bytes.
    /// @result The bytes, or null if the text is not valid base64 or is
    ///         the wrong size.
    [[nodiscard]] static std::optional<std::vector<unsigned char>>
        base64Decode(const std::string &text, const size_t expectedLength)
    {
        std::vector<unsigned char> result(expectedLength);
        size_t actualLength{0};
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

    /// @brief Registers a public key for the user.
    /// @result True on success; false if the user does not exist or the
    ///         key (name) is already registered.
    [[nodiscard]] bool addUserKey(const std::string &user,
                                  const std::string &keyName,
                                  const std::string &publicKey)
    {
        if (user.empty()){throw std::invalid_argument("User is empty");}
        if (keyName.empty()){throw std::invalid_argument("Key name is empty");}
        if (publicKey.empty())
        {
            throw std::invalid_argument("Public key is empty");
        }
        // Fail fast on junk before it lands in the database.
        if (!base64Decode(publicKey, crypto_sign_PUBLICKEYBYTES))
        {
            throw std::invalid_argument(
                "Public key is not a base64 ed25519 public key");
        }
        constexpr std::string_view query{"SELECT add_user_key($1, $2, $3)"};
        return executeScalar<bool> (query,
                                    pqxx::params{user, keyName, publicKey});
    }

    /// @brief Revokes the user's named key.
    /// @result True if an active key was revoked.
    [[nodiscard]] bool revokeUserKey(const std::string &user,
                                     const std::string &keyName)
    {
        if (user.empty()){throw std::invalid_argument("User is empty");}
        if (keyName.empty()){throw std::invalid_argument("Key name is empty");}
        constexpr std::string_view query{"SELECT revoke_user_key($1, $2)"};
        return executeScalar<bool> (query, pqxx::params{user, keyName});
    }

    // Get user by their (public) key.  Only active (unrevoked, unexpired)
    // keys belonging to a live account resolve to a user.
    [[nodiscard]] std::optional<std::string> getUserByKey(const std::string &publicKey) const
    {
        if (publicKey.empty())
        {
            throw std::invalid_argument("Public key is empty");
        }
        constexpr std::string_view query{"SELECT get_user_by_key($1)"};
        return executeOptionalScalar<std::string> (query,
                                                   pqxx::params{publicKey});
    }

    /// @brief Records a successful key authentication; also touches the
    ///        user's last_login.
    [[nodiscard]] bool recordKeyUse(const std::string &publicKey)
    {
        constexpr std::string_view query{"SELECT record_key_use($1)"};
        return executeScalar<bool> (query, pqxx::params{publicKey});
    }

    /// @brief Deletes every provisional account past its deadline.
    [[nodiscard]] int deleteExpiredProvisionalUsers()
    {
        constexpr std::string_view query{
            "SELECT delete_expired_provisional_users()"};
        return executeScalar<int> (query, pqxx::params{});
    }

    // Auth user by public key:
    // The user sends a message, a 64 byte detached signature of that
    // message, and their 32 byte public key (the latter two base64).
    // NOLINTBEGIN(bugprone-easily-swappable-parameters)
    [[nodiscard]] IAuthenticator::Result authenticateByKey(
        const std::string &message,
        const std::string &signature,
        const std::string &publicKey)
    // NOLINTEND(bugprone-easily-swappable-parameters)
    {
        // Is this an active key we know?
        auto user = getUserByKey(publicKey);
        if (!user)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "User not found for provided public key");
            return IAuthenticator::Result::InvalidCredentials;
        }
        // Unpack the key and signature
        auto publicKeyBytes
            = base64Decode(publicKey, crypto_sign_PUBLICKEYBYTES);
        auto signatureBytes = base64Decode(signature, crypto_sign_BYTES);
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
        if (!recordKeyUse(publicKey))
        {
            SPDLOG_LOGGER_WARN(mLogger, "Failed to record key use for {}",
                               *user);
        }
        SPDLOG_LOGGER_INFO(mLogger, "Verified {} by key", *user);
        return IAuthenticator::Result::Authenticated;
    }

    // Let's check this user's password
    [[nodiscard]] IAuthenticator::Result authenticateByPassword(
        const std::pair<std::string, std::string> &userAndPassword)
    {
        const auto &user = userAndPassword.first;
        const auto &password = userAndPassword.second;
        auto hashedPassword = getHashedPasswordFromDatabase(user);
        if (hashedPassword == std::nullopt)
        {
            // No such user, or a provisional account whose deadline has
            // passed.  Both are a rejection and the client is told which
            // in neither case - the difference is what lets someone
            // enumerate valid user names.
            SPDLOG_LOGGER_WARN(mLogger, "{} not in database", user);
            return IAuthenticator::Result::InvalidCredentials;
        }
        if (crypto_pwhash_str_verify(hashedPassword->c_str(),
                                     password.c_str(),
                                     password.length()) != 0)
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "{} provided incorrect password",
                               user);
            return IAuthenticator::Result::InvalidCredentials;
        }
        if (!recordLogin(user))
        {
            SPDLOG_LOGGER_WARN(mLogger,
                               "Failed to record login for {}", user);
        }
        // If the cost parameters have moved on since this hash was made,
        // re-hash at the current ones.  N.B. the comparison must be
        // against the limits this class actually hashes with; checking
        // against different ones makes every login look like it needs a
        // rehash and pays for a full argon2 hash and a database write on
        // every single one.
        if (crypto_pwhash_str_needs_rehash(hashedPassword->c_str(),
                                           mOperationsLimit,
                                           mMemoryLimit) != 0)
        {
            SPDLOG_LOGGER_INFO(mLogger, "Rehashing password for {}", user);
            try
            {
                if (!updatePassword(std::pair {user, password}))
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

    DatabaseOptions mOptions;
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    mutable std::mutex mDatabaseMutex;
    mutable std::unique_ptr<pqxx::connection> mConnection{nullptr};
    unsigned long long mOperationsLimit{crypto_pwhash_OPSLIMIT_MODERATE};
    std::size_t mMemoryLimit{crypto_pwhash_MEMLIMIT_MODERATE};
    bool mInitialized{false};
};

/// Constructor
Database::Database(const DatabaseOptions &options,
                   std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<DatabaseImpl> (options, std::move(logger)))
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
    return pImpl->getPermissions(user);
}

/// Still holding the password they were issued?
std::optional<bool>
Database::mustChangePassword(const std::string &user) const
{
    return pImpl->mustChangePassword(user);
}

/// Add a user
Database::AdminResult Database::addUser(
    const std::string &actor,
    const std::pair<std::string, std::string> &userNameAndPassword,
    const IAuthenticator::Permissions permissions)
{
    if (actor.empty()){throw std::invalid_argument("Actor is empty");}
    const auto &[user, password] = userNameAndPassword;
    if (user.empty()){throw std::invalid_argument("User is empty");}
    if (password.empty()){throw std::invalid_argument("Password is empty");}
    const auto permission = ::toStorablePermission(permissions);
    const auto hashedPassword = pImpl->hashPassword(password);
    constexpr std::string_view query{
        "SELECT admin_add_user($1, $2, $3, $4)"};
    return pImpl->executeAdmin(
        query,
        pqxx::params{actor, user, hashedPassword, permission},
        "admin_add_user");
}

/// Add a provisional user
Database::AdminResult Database::addProvisionalUser(
    const std::string &actor,
    const std::pair<std::string, std::string> &userNameAndPassword,
    const std::chrono::seconds &validFor,
    const IAuthenticator::Permissions permissions)
{
    if (actor.empty()){throw std::invalid_argument("Actor is empty");}
    const auto &[user, password] = userNameAndPassword;
    if (user.empty()){throw std::invalid_argument("User is empty");}
    if (password.empty()){throw std::invalid_argument("Password is empty");}
    if (validFor <= std::chrono::seconds {0})
    {
        throw std::invalid_argument("Validity duration must be positive");
    }
    const auto permission = ::toStorablePermission(permissions);
    const auto hashedPassword = pImpl->hashPassword(password);
    // make_interval rather than assembling an interval literal: the
    // duration crosses as a number and never as text to be re-parsed.
    constexpr std::string_view query{
        "SELECT admin_add_provisional_user($1, $2, $3, "
        "                                  make_interval(secs => $4), $5)"};
    return pImpl->executeAdmin(
        query,
        pqxx::params{actor, user, hashedPassword,
                     static_cast<long long> (validFor.count()), permission},
        "admin_add_provisional_user");
}

/// Change someone's level
Database::AdminResult Database::setUserPermission(
    const std::string &actor,
    const std::string &user,
    const IAuthenticator::Permissions permissions)
{
    if (actor.empty()){throw std::invalid_argument("Actor is empty");}
    if (user.empty()){throw std::invalid_argument("User is empty");}
    const auto permission = ::toStorablePermission(permissions);
    constexpr std::string_view query{
        "SELECT admin_set_user_permission($1, $2, $3)"};
    return pImpl->executeAdmin(query,
                               pqxx::params{actor, user, permission},
                               "admin_set_user_permission");
}

/// Reset a forgotten password
Database::AdminResult Database::resetUserPassword(
    const std::string &actor,
    const std::pair<std::string, std::string> &userNameAndPassword,
    const std::chrono::seconds &validFor)
{
    if (actor.empty()){throw std::invalid_argument("Actor is empty");}
    const auto &[user, password] = userNameAndPassword;
    if (user.empty()){throw std::invalid_argument("User is empty");}
    if (password.empty()){throw std::invalid_argument("Password is empty");}
    if (validFor <= std::chrono::seconds {0})
    {
        throw std::invalid_argument("Validity duration must be positive");
    }
    const auto hashedPassword = pImpl->hashPassword(password);
    constexpr std::string_view query{
        "SELECT admin_reset_user_password($1, $2, $3, "
        "                                 make_interval(secs => $4))"};
    return pImpl->executeAdmin(
        query,
        pqxx::params{actor, user, hashedPassword,
                     static_cast<long long> (validFor.count())},
        "admin_reset_user_password");
}

/// Remove a user
Database::AdminResult Database::removeUser(const std::string &actor,
                                           const std::string &user)
{
    if (actor.empty()){throw std::invalid_argument("Actor is empty");}
    if (user.empty()){throw std::invalid_argument("User is empty");}
    constexpr std::string_view query{"SELECT admin_remove_user($1, $2)"};
    return pImpl->executeAdmin(query, pqxx::params{actor, user},
                               "admin_remove_user");
}

/// Update a password
bool Database::updatePassword(
    const std::pair<std::string, std::string> &userNameAndPassword)
{
    return pImpl->updatePassword(userNameAndPassword);
}

/// Sweep the expired provisional accounts
int Database::deleteExpiredProvisionalUsers()
{
    return pImpl->deleteExpiredProvisionalUsers();
}

/// Register a key
bool Database::addUserKey(const std::string &user,
                          const std::string &keyName,
                          const std::string &publicKey)
{
    return pImpl->addUserKey(user, keyName, publicKey);
}

/// Revoke a key
bool Database::revokeUserKey(const std::string &user,
                             const std::string &keyName)
{
    return pImpl->revokeUserKey(user, keyName);
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

/// Connected?
bool Database::isConnected() const noexcept
{
    return pImpl->isConnected();
}

/// Destructor
Database::~Database() = default;

#include <stdlib.h>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#ifndef NDEBUG
#include <cassert>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#endif
extern "C" 
{
#include <ldap.h>
}
#include "aqmsDutyReviewBackend/auth/openldap.hpp"
#include "aqmsDutyReviewBackend/auth/authenticator.hpp"
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"

using namespace AQMSDutyReviewBackend::Auth;

class OpenLDAP::OpenLDAPImpl
{
public:
    explicit OpenLDAPImpl(const OpenLDAPOptions &options,
                          std::shared_ptr<spdlog::logger> logger) :
        mOptions(options),
        mLogger(std::move(logger))
    {
        if (!mOptions.hasHost())
        {
            throw std::runtime_error("LDAP host not set");
        }
        if (mLogger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
            constexpr const char *loggerName{"OpenLDAPConsole"};
            mLogger = spdlog::get(loggerName);
            if (mLogger == nullptr)
            {   
                mLogger = spdlog::stdout_color_mt(loggerName);
            }   
            // NOLINTEND(misc-include-cleaner)
        }
        setTLSEnvironmentVariable();
        mInitialized = true;
    }

    void setTLSEnvironmentVariable()
    {
        auto verify = mOptions.getTLSVerifyClient(); 
        if (verify != std::nullopt)
        {
            constexpr int overwrite{1};
            if (*verify == OpenLDAPOptions::TLSVerifyClient::Never)
            {
                if (setenv("LDAPTLS_REQCERT", "NEVER", overwrite) != 0)
                {
                    throw std::runtime_error(
                       "LDAP: Failed to update LDAPTLS_REQCERT to NEVER");
                }
            }
            else if (*verify == OpenLDAPOptions::TLSVerifyClient::Allow)
            {
                if (setenv("LDAPTLS_REQCERT", "ALLOW", overwrite) != 0)
                {
                    throw std::runtime_error(
                       "LDAP: Failed to update LDAPTLS_REQCERT to ALLOW");
                } 
            }
            else if (*verify == OpenLDAPOptions::TLSVerifyClient::Try)
            {
                if (setenv("LDAPTLS_REQCERT", "TRY", overwrite) != 0)
                {
                    throw std::runtime_error(
                       "LDAP: Failed to update LDAPTLS_REQCERT to TRY");
                }
            }
            else if (*verify == OpenLDAPOptions::TLSVerifyClient::Demand)
            {
                if (setenv("LDAPTLS_REQCERT", "DEMAND", overwrite) != 0)
                {
                    throw std::runtime_error(
                       "LDAP: Failed to update LDAPTLS_REQCERT to DEMAND");
                }
            }
            else
            {
                throw std::runtime_error("Unhandled TLS cert option");
            }
        }
    }

    void unbind()
    {
        if (!mInitialized)
        {
            throw std::runtime_error("Class not initialized (unbind)");
        }
        if (mBound)
        {
            auto returnCode
                = ldap_unbind_ext(mLDAP, &mServerControl, &mClientControl);
            if (returnCode != LDAP_SUCCESS)
            {
                const std::string error{ldap_err2string(returnCode)};
                throw std::runtime_error("Failed to unbind OpenLDAP because "
                                        + error);
            }
            mBound = false;
        }
    }

    void bind()
    {
        if (!mInitialized)
        {
            throw std::runtime_error("Class not initialized (bind)");
        }
        unbind();
        auto address = mOptions.getAddress();
        if (ldap_initialize(&mLDAP, address.c_str()) != LDAP_SUCCESS)
        {
            mBound = false;
            throw std::runtime_error("Failed to bind to LDAP at " 
                                   + address);
        }
        // Set the version
        auto ldapVersion{LDAP_VERSION3};
        auto version = mOptions.getVersion();
        if (version == OpenLDAPOptions::Version::One)
        {
            ldapVersion = LDAP_VERSION1;
        }
        else if (version == OpenLDAPOptions::Version::Two)
        {
            ldapVersion = LDAP_VERSION2;
        }
        else if (version == OpenLDAPOptions::Version::Three)
        {
            ldapVersion = LDAP_VERSION3;
        }
        else
        {
            throw std::runtime_error("Unhandled LDAP version");
        }
        auto returnCode = ldap_set_option(mLDAP,
                                          LDAP_OPT_PROTOCOL_VERSION,
                                          &ldapVersion);
        if (returnCode != LDAP_SUCCESS)
        {
            unbind();
            throw std::runtime_error("Failed to set LDAP version");
        }
        mBound = true;
    }

    [[nodiscard]] IAuthenticator::Result authenticate(
        const std::pair<std::string, std::string> &userAndPassword)
    {
        IAuthenticator::Result result{IAuthenticator::Result::InvalidCredentials};
        if (!mInitialized)
        {
            throw std::runtime_error("Class not initialized (authenticate)");
        }
        // Setup server cred
        auto user = userAndPassword.first;
        if (user.empty()){throw std::invalid_argument("User not set");}
        // NOLINTNEXTLINE(misc-const-correctness, misc-include-cleaner)
        struct berval *serverCredential{nullptr};
        auto dn = "uid=" + user;
        auto suffix = mOptions.getSuffix();
        if (!suffix.empty()){dn = dn + "," + suffix;}
        // Setup client cred
        std::string temporaryPassword{userAndPassword.second};
        //NOLINTNEXTLINE(misc-include-cleaner)
        struct berval credential;
        credential.bv_val = temporaryPassword.data();
        credential.bv_len = temporaryPassword.size();
        auto maintainConnection = mOptions.maintainConnection();
        std::string message;
        // Okay let's auth this guy
        {
        const std::lock_guard<std::mutex> lock(mMutex);
        if (!maintainConnection)
        {
            bind();
        }
        else
        {
            if (!mBound){bind();}
        }
        if (!mBound)
        {
            throw std::runtime_error("Failed to bind to LDAP");
        }  
        // SASL auth
        auto returnCode
            = ldap_sasl_bind_s(mLDAP,
                               dn.c_str(),
                               LDAP_SASL_SIMPLE,
                               &credential,
                               nullptr,
                               nullptr,
                               &serverCredential);
        // Let's make sure we finish off the connection before moving forward
        if (!maintainConnection){unbind();}
        // Check the result
        if (returnCode != LDAP_SUCCESS)
        {
            if (returnCode != LDAP_INVALID_CREDENTIALS)
            {
                const std::string error{ldap_err2string(returnCode)};
                message = "Could not bind to SASL because " + error;
                result = IAuthenticator::Result::ServerError;
            }
            else
            {
                message = user + " had invalid credentials";
                result = IAuthenticator::Result::InvalidCredentials;
            }
        }
        else
        {
            message = user + " authenticated"; 
            result = IAuthenticator::Result::Authenticated;
        }
        }
        if (mLogger)
        {
            if (result == IAuthenticator::Result::ServerError)
            {
                SPDLOG_LOGGER_ERROR(mLogger, "{}", message);
            }
            else
            {
                SPDLOG_LOGGER_INFO(mLogger, "{}", message);
            }
        }
        return result;
    }

    ~OpenLDAPImpl()
    {
        unbind();
    }

//private:
    OpenLDAPOptions mOptions; 
    std::shared_ptr<spdlog::logger> mLogger{nullptr};
    std::mutex mMutex;
    LDAP *mLDAP{nullptr};
    LDAPControl *mClientControl{nullptr};
    LDAPControl *mServerControl{nullptr};
    bool mInitialized{false};
    bool mBound{false};
};

/// Constructor
OpenLDAP::OpenLDAP(const OpenLDAPOptions &options) :
    //IAuthenticator(),
    pImpl(std::make_unique<OpenLDAPImpl> (options, nullptr))
{
}

/// Constructor
OpenLDAP::OpenLDAP(const OpenLDAPOptions &options,
                   std::shared_ptr<spdlog::logger> logger) :
    pImpl(std::make_unique<OpenLDAPImpl> (options, std::move(logger)))
{
}

/// Authenticate the user
IAuthenticator::Result OpenLDAP::authenticateBasic(
    const std::pair<std::string, std::string> &userNameAndPassword)
{
    if (!isInitialized()){throw std::runtime_error("LDAP auth not initialized");}
    return pImpl->authenticate(userNameAndPassword);
}

bool OpenLDAP::isInitialized() const noexcept
{
    return pImpl->mInitialized;
}

/// Destructor
OpenLDAP::~OpenLDAP() = default;


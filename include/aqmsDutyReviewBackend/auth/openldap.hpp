#ifndef AQMS_DUTY_REVIEW_BACKEND_AUTH_OPENLDAP_HPP
#define AQMS_DUTY_REVIEW_BACKEND_AUTH_OPENLDAP_HPP
#include <memory>
#include <spdlog/logger.h>
#include "aqmsDutyReviewBackend/auth/authenticator.hpp"
namespace AQMSDutyReviewBackend::Auth
{
 class OpenLDAPOptions;
}

namespace AQMSDutyReviewBackend::Auth
{

/// @class OpenLDAP "openldap.hpp"
/// @brief Authenticates a user via OpenLDAP.
/// @coyright Ben Baker (University of Utah) distributed under the
///           MIT NO AI license.
class OpenLDAP final : public IAuthenticator
{
public:
    /// @brief Initializes the OpenLDAP client.
    explicit OpenLDAP(const OpenLDAPOptions &options);
    /// @brief Initializes the OpenLDAP client with a logger
    OpenLDAP(const OpenLDAPOptions &options,
             std::shared_ptr<spdlog::logger> logger);

    /// @brief Authenticates the user based on their credentials.
    [[nodiscard]] IAuthenticator::Result authenticateBasic(const std::pair<std::string, std::string> &userNameAndPassword) override final;

    /// 
    //[[nodiscard]] IAuthenticator::Result operator()(const std::string &jwt) const final;

    /// @result True indicates the class is initialized.
    [[nodiscard]] bool isInitialized() const noexcept;
    /// @brief Destructor.
    virtual ~OpenLDAP() final;

    OpenLDAP() = delete;
    OpenLDAP(const OpenLDAP &) = delete;
    OpenLDAP(OpenLDAP &&) noexcept = delete;
    OpenLDAP& operator=(const OpenLDAP &) = delete;
    OpenLDAP& operator=(OpenLDAP &&) noexcept = delete;
private:
    class OpenLDAPImpl;
    std::unique_ptr<OpenLDAPImpl> pImpl;
};
}
#endif

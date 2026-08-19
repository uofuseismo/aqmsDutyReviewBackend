#ifndef AQMS_DUTY_REVIEW_BACKEND_AUTH_OPENLDAP_OPTIONS_HPP
#define AQMS_DUTY_REVIEW_BACKEND_AUTH_OPENLDAP_OPTIONS_HPP
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
namespace AQMSDutyReviewBackend::Auth
{
/// @class OpenLDAPOptions
/// @brief Defines the options for connecting to the server running LDAP
///        via OpenLDAP.
/// @copyright Ben Baker (University of Utah) distributed under the MIT NO
///            AI license.
class OpenLDAPOptions
{
public:
    /// @brief Defines the LDAP version protocol.
    enum class Version
    {   
        One = 1,
        Two = 2,
        Three = 3
    };
    /// @brief The directive specifies what checks to perform on client 
    ///        certificates in an incoming TLS session.
    enum class TLSVerifyClient
    {
        Never, /*!< Never ask for the client certificate.  This is the default. */
        Allow, /*!< The server will ask for a client certificate.  If none is
                    provided then the session proceeds normally.  If a certificate
                    is provided but the server cannot verify it, the certficate is
                    ignored and the session proceeds normally as if no
                    certificate had been provided. */
        Try,   /*!< The certificate is requested and if none is provided the
                    session proceeds normally.  If a certificate is provided
                    and it cannot be verfied then the session is immediately
                    terminated. */
        Demand /*!< The certificate must be provided and validated otherwise
                    the session is immediately terminated. */
    }; 
public:
    /// @brief Constructor.
    OpenLDAPOptions(); 
    /// @brief Copy constructor.
    OpenLDAPOptions(const OpenLDAPOptions &options);
    /// @brief Move constructor.
    OpenLDAPOptions(OpenLDAPOptions &&options) noexcept;

    /// @brief Sets the LDAP server's host.
    /// @param[in] host   The host name - e.g., server.domain.com.
    void setHost(const std::string &host);
    /// @result The host.
    /// @throws std::runtime_error if \c hasHost() is false.
    [[nodiscard]] std::string getHost() const;
    /// @result True indicates the host was set.
    [[nodiscard]] bool hasHost() const noexcept;

    /// @brief Sets the port.  
    /// @param[in] port  The port - this is typically 389 for unencrypted
    ///                  sessions and 636 for encrypted sessions.
    void setPort(uint16_t port);
    /// @result The port number.
    /// @note By default this is 636.
    [[nodiscard]] uint16_t getPort() const noexcept;

    /// @brief Sets the organziational unit name.
    /// @param[in] unit   The organizational unit - e.g., ou=Groups.
    void setOrganizationalUnit(const std::string &unit);
    /// @result The organizational unit.
    [[nodiscard]] std::string getOrganizationalUnit() const noexcept;

    /// @brief Sets the domain component.
    /// @param[in] component   The domain component - e.g., dc=gl,dc=google,dc=com
    void setDomainComponent(const std::string &component);
    /// @result The domain component.
    [[nodiscard]] std::string getDomainComponent() const noexcept;

    /// @brief Maintains an open connection with the LDAP server.  
    /// @note This is not a good idea - the application will likely
    ///       crash if the that LDAP server is stopped.
    void enableMaintainConnection() noexcept;
    /// @brief Each authentication will require forming a new connection
    ///        to an LDAP server.
    /// @note We typically have very low traffic so this is a much more 
    ///       robust solution than maintaining connection and is usually
    ///       fast enough.
    void disableMaintainConnection() noexcept;
    /// @brief If true then the connection is maintained.
    /// @note The default is false.
    [[nodiscard]] bool maintainConnection() const noexcept;

    /// @brief Sets the OpenLDAP version protocol.
    void setVersion(Version version) noexcept;
    /// @result The OpenLDAP version protocol. 
    /// @note By default this is 3.
    [[nodiscard]] Version getVersion() const noexcept;

    /// @brief Sets the TLS behavior.
    void setTLSVerifyClient(TLSVerifyClient verify);
    /// @result The TLS client verification behavior.
    /// @note The default is std::nullopt.
    std::optional<TLSVerifyClient> getTLSVerifyClient() const noexcept;

    /// @brief Indicates we're using ldaps as a prefix in the ldap address.
    void enableSecured() noexcept;
    /// @brief Indicates we're using ldap as a prefix in the ldap address.
    void disableSecured() noexcept; 
    [[nodiscard]] bool isSecured() const noexcept;

    /// @result The corresonding address. 
    /// @throws std::runtime_error if \c hasHost() is false.
    [[nodiscard]] std::string getAddress() const;
    /// @result The corresponding suffix - e.g., organizationUnit,domainComponent.
    [[nodiscard]] std::string getSuffix() const noexcept;

    /// @brief Copy assignment.
    OpenLDAPOptions &operator=(const OpenLDAPOptions &options);
    /// @brief Move assignment.
    OpenLDAPOptions &operator=(OpenLDAPOptions &&options) noexcept;
    /// @brief Destructor.
    ~OpenLDAPOptions();

    /// @brief Creates LDAP options from an initialization file.
    /// @param[in] file     The ini file path.
    /// @param[in] section  The section of the ini file to read e.g. 
    ///                     to read the [Database] section specify
    ///                     "Database".
    static OpenLDAPOptions fromInitializationFile(const std::filesystem::path &file,
                                                  const std::string &section = "");

private:
    class OpenLDAPOptionsImpl;
    std::unique_ptr<OpenLDAPOptionsImpl> pImpl;
};
}
#endif

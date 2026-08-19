#include <algorithm>
#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"

using namespace AQMSDutyReviewBackend::Auth;

class OpenLDAPOptions::OpenLDAPOptionsImpl
{
public:
    void updateAddress()
    {
        if (!mHost.empty())
        {
             if (mSecured)
             {
                 mAddress = "ldaps://";
             }
             else
             {
                 mAddress = "ldap://";
             }
             mAddress.append(mHost);
             mAddress.append(":");
             mAddress.append(std::to_string(mPort));
        }
    }

    void updateSuffix()
    {
        mSuffix.clear();
        if (!mOrganizationalUnit.empty())
        {
            mSuffix = mOrganizationalUnit;
        }
        if (!mDomainComponent.empty())
        {
            if (mSuffix.empty())
            {
                mSuffix = mDomainComponent;
            }
            else
            {
                mSuffix = mSuffix + "," + mDomainComponent;
            }
        } 
    } 

    std::string mAddress;
    std::string mSuffix;
    std::string mHost;
    std::string mOrganizationalUnit;
    std::string mDomainComponent;
    OpenLDAPOptions::Version mVersion{OpenLDAPOptions::Version::Three};
    std::optional<TLSVerifyClient> mTLSVerify{std::nullopt};
    uint16_t mPort{636};
    bool mMaintainConnection{false};
    bool mSecured{true};
};

/// Constructor
OpenLDAPOptions::OpenLDAPOptions() :
    pImpl(std::make_unique<OpenLDAPOptionsImpl> ())
{
}

/// Copy constructor
OpenLDAPOptions::OpenLDAPOptions(const OpenLDAPOptions &options)
{
    *this = options;
}

/// Move constructor
OpenLDAPOptions::OpenLDAPOptions(OpenLDAPOptions &&options) noexcept
{
    *this = std::move(options);
}

/// Copy assignment
OpenLDAPOptions& OpenLDAPOptions::operator=(const OpenLDAPOptions &options)
{
    if (&options == this){return *this;} 
    pImpl = std::make_unique<OpenLDAPOptionsImpl> (*options.pImpl);
    return *this;
}

/// Move assignment
OpenLDAPOptions& OpenLDAPOptions::operator=(OpenLDAPOptions &&options) noexcept
{
    if (&options == this){return *this;}
    pImpl = std::move(options.pImpl);
    return *this;
}
    
/// Destructor
OpenLDAPOptions::~OpenLDAPOptions() = default;

/// Host
void OpenLDAPOptions::setHost(const std::string &host)
{
    if (host.empty())
    {
        throw std::invalid_argument("Host is empty");
    }
    pImpl->mHost = host;
    pImpl->updateAddress();
}

std::string OpenLDAPOptions::getHost() const
{
    if (!hasHost()){throw std::runtime_error("Host not set");}
    return pImpl->mHost;
}

bool OpenLDAPOptions::hasHost() const noexcept
{
    return !pImpl->mHost.empty();
}

/// Port
void OpenLDAPOptions::setPort(const uint16_t port)
{
    if (port == 0){throw std::invalid_argument("Port cannot be zero");}
    pImpl->mPort = port;
    pImpl->updateAddress();
}

uint16_t OpenLDAPOptions::getPort() const noexcept
{
    return pImpl->mPort;
}

/// Get the address
std::string OpenLDAPOptions::getAddress() const
{
    if (!hasHost()){throw std::runtime_error("Host not set");}
    return pImpl->mAddress;
}

/// Maintain connection
void OpenLDAPOptions::enableMaintainConnection() noexcept
{
    pImpl->mMaintainConnection = true;
}

void OpenLDAPOptions::disableMaintainConnection() noexcept
{
    pImpl->mMaintainConnection = false;
}

bool OpenLDAPOptions::maintainConnection() const noexcept
{
    return pImpl->mMaintainConnection;
}

/// Domain component
void OpenLDAPOptions::setDomainComponent(const std::string &component)
{
    pImpl->mDomainComponent = component;
    pImpl->updateSuffix();
}

std::string OpenLDAPOptions::getDomainComponent() const noexcept
{
    return pImpl->mDomainComponent;
}

/// Organizational unit
void OpenLDAPOptions::setOrganizationalUnit(const std::string &unit)
{
    pImpl->mOrganizationalUnit = unit;
    pImpl->updateSuffix();
}

std::string OpenLDAPOptions::getOrganizationalUnit() const noexcept
{
    return pImpl->mOrganizationalUnit;
}

/// Version
void OpenLDAPOptions::setVersion(const Version version) noexcept
{
    pImpl->mVersion = version;
}

OpenLDAPOptions::Version OpenLDAPOptions::getVersion() const noexcept
{
    return pImpl->mVersion;
}

void OpenLDAPOptions::setTLSVerifyClient(const TLSVerifyClient verify)
{
    pImpl->mTLSVerify = std::make_optional (verify);
}

std::optional<OpenLDAPOptions::TLSVerifyClient> 
OpenLDAPOptions::getTLSVerifyClient() const noexcept
{
    return pImpl->mTLSVerify;
}

/// Suffix
std::string OpenLDAPOptions::getSuffix() const noexcept
{
    return pImpl->mSuffix;
}

/// Secured
void OpenLDAPOptions::enableSecured() noexcept
{
    pImpl->mSecured = true;
}

void OpenLDAPOptions::disableSecured() noexcept
{
    pImpl->mSecured = false;
}

bool OpenLDAPOptions::isSecured() const noexcept
{
    return pImpl->mSecured;
}

///--------------------------------------------------------------------------///
///                         Build from ini file                              ///
///--------------------------------------------------------------------------///
#include <filesystem>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>

OpenLDAPOptions OpenLDAPOptions::fromInitializationFile(
    const std::filesystem::path &iniFile,
    const std::string &sectionIn)
{
    if (!std::filesystem::exists(iniFile))
    {   
        throw std::invalid_argument("Initialization " + std::string{iniFile}
                                  + " file does not exist");
    }   
    // Make sure section ends with . so we can find stuff
    auto section = sectionIn;
    if (!section.empty() && section.back() != '.'){section.append(".");}

    // Parse the initialization file
    boost::property_tree::ptree propertyTree;
    boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);

    OpenLDAPOptions result;

    auto host = propertyTree.get<std::string> (section + "host");
    result.setHost(host);

    auto port
        = propertyTree.get<uint16_t> (section + "port",
                                      result.getPort());
    result.setPort(port);
   
    auto ou
        = propertyTree.get<std::string> (section + "organizationalUnit",
                                         result.getOrganizationalUnit());
    if (!ou.empty()){result.setOrganizationalUnit(ou);}

    auto dc
        = propertyTree.get<std::string> (section + "domainComponent",
                                         result.getDomainComponent());
    if (!dc.empty()){result.setDomainComponent(dc);}

    auto version = static_cast<int> (result.getVersion());
    version = propertyTree.get<int> (section + "version", version);
    if (version < 1 || version > 3)
    {
        throw std::invalid_argument("Invalid version");
    }
    result.setVersion(static_cast<OpenLDAPOptions::Version> (version));

    auto maintainConnection
          = propertyTree.get<bool> (section + "maintainConnection",
                                    result.maintainConnection());
    if (maintainConnection)
    {
        result.enableMaintainConnection();
    }
    else
    {
        result.disableMaintainConnection();
    }

    auto isSecured = propertyTree.get<bool> (section + "isSecured",
                                             result.isSecured());
    if (isSecured)
    {
        result.enableSecured();
    }
    else
    {
        result.disableSecured();
    }

    auto tlsVerify 
         = propertyTree.get<std::string> (section + "tlsVerify", "allow");
    if (!tlsVerify.empty())
    {
        std::transform(tlsVerify.begin(), tlsVerify.end(),
                       tlsVerify.begin(), ::tolower); 
        if (tlsVerify == "allow")
        {
            result.setTLSVerifyClient(OpenLDAPOptions::TLSVerifyClient::Allow);
        }
        else if (tlsVerify == "never")
        {
            result.setTLSVerifyClient(OpenLDAPOptions::TLSVerifyClient::Never);
        }
        else if (tlsVerify == "try")
        {
            result.setTLSVerifyClient(OpenLDAPOptions::TLSVerifyClient::Try);
        }
        else if (tlsVerify == "demand")
        {
            result.setTLSVerifyClient(OpenLDAPOptions::TLSVerifyClient::Demand);
        }
        else
        {
            throw std::invalid_argument(
                "tlsVerify can be never, allow, try, or demand but got "
              + tlsVerify);
        }
    }

    return result;
}

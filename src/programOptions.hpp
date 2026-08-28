#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#ifdef CROW_ENABLE_COMPRESSION
#include <crow/compression.h>
#endif
#include "aqmsDutyReviewBackend/auth/databaseOptions.hpp"
#include "aqmsDutyReviewBackend/auth/jsonWebTokenOptions.hpp"
/*
#ifdef WITH_OPENLDAP
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"
#endif
*/
#include "aqmsDutyReviewBackend/database/credentials.hpp"
#include "otelOptions.hpp"

#define APPLICATION_NAME "aqmsDutyReviewBackend"
namespace
{

/// @brief Site policy for how long provisional credentials live.
/// @note These are policy rather than mechanism, which is why they are
///       configured and not compiled in: how long somebody gets to turn up
///       and activate an account is an operational decision, and it
///       differs between a lab and a 24-hour operations room.
struct UserManagementOptions
{
    static UserManagementOptions
        parseInitializationFile(boost::property_tree::ptree &propertyTree,
                                const std::string &sectionIn = "UserManagement")
    {
        auto section = sectionIn;
        if (section.empty()){section = "UserManagement";}
        if (section.back() != '.'){section.append(".");}

        UserManagementOptions result;

        auto newAccountHours
            = propertyTree.get<int>
              (section + "provisionalAccountExpiresAfterNHours",
               static_cast<int> (result.provisionalAccountExpiresAfter.count()));
        if (newAccountHours <= 0)
        {
            // A non-positive lifetime creates an account that is already
            // expired: the database refuses the interval, and the operator
            // is left wondering why hiring somebody failed.
            throw std::invalid_argument(
                "provisionalAccountExpiresAfterNHours must be positive");
        }
        result.provisionalAccountExpiresAfter
            = std::chrono::hours {newAccountHours};

        auto resetHours
            = propertyTree.get<int>
              (section + "passwordResetExpiresAfterNHours",
               static_cast<int> (result.passwordResetExpiresAfter.count()));
        if (resetHours <= 0)
        {
            throw std::invalid_argument(
                "passwordResetExpiresAfterNHours must be positive");
        }
        result.passwordResetExpiresAfter = std::chrono::hours {resetHours};

        return result;
    }

    /// How long a newly created account's temporary password stays usable.
    /// A week, by default - long enough to survive somebody starting on a
    /// Friday.
    std::chrono::hours provisionalAccountExpiresAfter{24*7};
    /// How long a reset password stays usable.  A day, by default: a reset
    /// is answering somebody who is locked out right now, and the window
    /// during which an administrator holds a working credential for
    /// somebody else's account should be short.
    std::chrono::hours passwordResetExpiresAfter{24};
};

struct CrowOptions
{
    static CrowOptions 
        parseInitializationFile(boost::property_tree::ptree &propertyTree,
                                const std::string &sectionIn = "Crow")
    {
        auto section = sectionIn;
        if (section.empty()){section = "Crow";}
        if (section.back() != '.'){section.append(".");} 
        CrowOptions options;
        options.bindAddress
            = propertyTree.get<std::string> (section + "address",
                                             options.bindAddress);
        if (options.bindAddress.empty())
        {
            throw std::invalid_argument("Crow bind address is empty");
        }

        options.port
            = propertyTree.get<uint16_t> (section + "port",
                                          options.port);

        options.serverName
            = propertyTree.get<std::string> (section + "serverName",
                                             options.serverName);
        if (options.serverName.empty())
        {
            throw std::invalid_argument("Crow server name is empty");
        }

        options.nThreads 
            = propertyTree.get<uint16_t>
                  (section + "numberOfThreads",
                   static_cast<uint16_t> (options.nThreads));
        if (options.nThreads < 1)
        {
            throw std::invalid_argument("Crow number of threads not positive");
        }

#ifdef CROW_ENABLE_COMPRESSION
        auto compressionType
           = propertyTree.get<std::string> (section + "compression",
                                            "none");
        std::transform(compressionType.begin(),
                       compressionType.end(),
                       compressionType.begin(),
                       ::tolower);
        if (compressionType == "none")
        {
            options.useCompression = false;
        }
        else if (compressionType == "zlib")
        {
            options.useCompression = true;
            options.compression = crow::compression::algorithm::DEFLATE;
        }
        else if (compressionType == "gzip")
        {
            options.useCompression = true;
            options.compression = crow::compression::algorithm::GZIP;
        }
        else
        {
            throw std::invalid_argument("Unhandled compression type: "
                                       + compressionType);
        }
#endif
#ifdef CROW_ENABLE_SSL
        auto certificateFile
            = propertyTree.get_optional<std::string>
              (section + "certificateFile");
        auto certificateKey
            = propertyTree.get_optional<std::string>
              (section + "certificateKey");
        if (certificateFile && certificateKey)
        {
            if (!std::filesystem::exists(*certificateFile))
            {
                throw std::invalid_argument("Certificate "
                                          + *certificateFile 
                                          + " does not exist");
            }
            if (!std::filesystem::exists(*certificateKey))
            {
                throw std::invalid_argument("Certificate "
                                          + *certificateKey
                                          + " does not exist");
            }
            options.certificateAndKeyFile
                = std::make_pair(*certificateFile, *certificateKey); 
            options.useCertificateChain = false;
            options.useSSL = true;
        } 
        else
        {
            auto certificateChainFile 
               = propertyTree.get_optional<std::string> 
                 (section + "certificateChainFile");
            auto certificateChainKey 
               = propertyTree.get_optional<std::string> 
                 (section + "certificateChainKey");
            if (certificateChainFile && certificateChainKey)
            {
                if (!std::filesystem::exists(*certificateChainFile))
                {
                    throw std::invalid_argument("Certificate chain "
                                              + *certificateChainFile
                                              + " does not exist");
                }
                if (!std::filesystem::exists(*certificateChainKey))
                {
                    throw std::invalid_argument("Certificate chain "
                                              + *certificateChainKey
                                              + " does not exist");
                }
                options.certificateAndKeyFile
                    = std::make_pair(*certificateChainFile,
                                     *certificateChainKey);
                options.useCertificateChain = true;
                options.useSSL = true;
            }
        }
#endif
        return options;
    }
    std::string bindAddress{"127.0.0.1"};
    std::string serverName{"aqms-drp-server"};
    int nThreads{1};
    uint16_t port{8080};
#ifdef CROW_ENABLE_COMPRESSION
    crow::compression::algorithm compression = crow::compression::algorithm::GZIP;
    bool useCompression{true};
#else
    bool useCompression{false};
#endif
#ifdef CROW_ENABLE_SSL
    std::pair<std::filesystem::path, std::filesystem::path> certificateAndKeyFile;
    bool useCertificateChain{false};
    bool useSSL{false};
#else
    bool useSSL{false};
#endif
};

struct ProgramOptions
{
    void parseInitializationFile(const std::filesystem::path &iniFile)
    {   
        namespace DRP = AQMSDutyReviewBackend;
        if (!std::filesystem::exists(iniFile))
        {
            throw std::invalid_argument("Initialization file "
                                      + std::string {iniFile}
                                      + " does not exist");
        }
        // Parse the initialization file
        boost::property_tree::ptree propertyTree;
        boost::property_tree::ini_parser::read_ini(iniFile, propertyTree);
        applicationName
            = propertyTree.get<std::string> ("General.applicationName",
                                             applicationName);
        if (applicationName.empty()){applicationName = APPLICATION_NAME;}
        verbosity
            = propertyTree.get<int> ("General.verbosity", verbosity);

/*
        auto printSummaryIntervalInMinutes
            = propertyTree.get<int> ("General.printSummaryIntervalInMinutes",
                                     PRINT_SUMMARY_INTERVAL_MINUTES);
        printSummaryInterval = std::chrono::minutes {printSummaryIntervalInMinutes};
*/
/*
#ifdef WITH_OPENLDAP
        // OpenLDAP 
        openLDAPOptions
            = DRP::Auth::OpenLDAPOptions::fromInitializationFile(
                iniFile, "OpenLDAP");
#endif
*/
        // DRP database
        auto drpDatabaseCredentials
            = DRP::Database::Credentials::fromInitializationFile(
                iniFile, "DRP");
        if (drpDatabaseCredentials.isReadOnly())
        {
            throw std::invalid_argument(
                "DRP database requires read/write connection");
        }
        if (!drpDatabaseCredentials.hasUser())
        {
            throw std::invalid_argument("DRP database user name not set");
        }
        if (!drpDatabaseCredentials.hasPassword())
        {
            throw std::invalid_argument("DRP database password not set");
        }
        drpDatabaseOptions.setCredentials(drpDatabaseCredentials);

        // Crow
        crowOptions
            = CrowOptions::parseInitializationFile(propertyTree, "Crow");

        userManagementOptions
            = UserManagementOptions::parseInitializationFile(propertyTree,
                                                             "UserManagement");

        // AQMS database
        aqmsDatabaseCredentials
            = DRP::Database::Credentials::fromInitializationFile(
                iniFile, "AQMS");
        if (!aqmsDatabaseCredentials.hasUser())
        {
            throw std::invalid_argument("AQMS database user name not set");
        }
        if (!aqmsDatabaseCredentials.hasPassword())
        {
            throw std::invalid_argument("AQMS database password not set");
        }

        // Aux databases
        for (int16_t i = 1; i < std::numeric_limits<int16_t>::max(); ++i)
        {
            auto section = "AQMSRemote" + std::to_string(i);
            if (propertyTree.get_optional<std::string> (section))
            {
                auto otherAQMSDatabaseCredentials 
                   = DRP::Database::Credentials::fromInitializationFile(
                        iniFile, section);
                if (!otherAQMSDatabaseCredentials.hasUser())
                {
                    throw std::invalid_argument(
                        section + " does not specify user name");
                }
                if (!otherAQMSDatabaseCredentials.hasPassword())
                { 
                    throw std::invalid_argument(
                        section + " does not specify password");
                }
                // This will throw
                auto connectionString
                    = otherAQMSDatabaseCredentials.getConnectionString();
                for (const auto &credentials : auxiliaryAQMSCredentials)
                {
                    if (credentials.getConnectionString() == connectionString)
                    {
                        throw std::invalid_argument("Duplicate connection for "
                                                  + section);
                    }
                }
                // Kept, not just validated - these were being parsed,
                // checked, and then dropped on the floor, so a configured
                // ancillary machine never reached the application.
                //NOLINTBEGIN(performance-inefficient-vector-operation)
                auxiliaryAQMSCredentials.push_back(
                    std::move(otherAQMSDatabaseCredentials));
                //NOLINTEND(performance-inefficient-vector-operation)
            }
            else
            {
                break;
            }
        }
             
        // JWT Auth 
        jsonWebTokenOptions
            = DRP::Auth::JSONWebTokenOptions::fromInitializationFile(
                iniFile, "Authentication");

        // Get OTel logs options
        auto httpLog
            = DRP::OTelOptions::getHTTPLogOptionsFromIniFile(
                propertyTree, "OTelHTTPLogOptions");
        exportLogs = false;
        if (httpLog != std::nullopt)
        {
            otelHTTPLogOptions = *httpLog;
            exportLogs = true;
            exportLogsWithHTTP = true;
        }
        else
        {
            auto grpcLog
                = DRP::OTelOptions::getGRPCLogOptionsFromIniFile(
                    propertyTree, "OTelGRPCLogOptions");
            if (grpcLog != std::nullopt)
            {   
                otelGRPCLogOptions = *grpcLog;
                exportLogs = true;
                exportLogsWithHTTP = false;
            }
        }

        // Get OTel metrics options
        auto httpMetrics
            = DRP::OTelOptions::getHTTPMetricsOptionsFromIniFile(
                propertyTree, "OTelHTTPMetricsOptions");
        exportMetrics = false;
        if (httpMetrics != std::nullopt)
        {
            otelHTTPMetricsOptions = *httpMetrics;
            exportMetrics = true;
            exportMetricsWithHTTP = true;
        }
        else
        {
            auto grpcMetrics
                = DRP::OTelOptions::getGRPCMetricsOptionsFromIniFile(
                    propertyTree, "OTelGRPCMetricsOptions");
            if (grpcMetrics != std::nullopt)
            {
                otelGRPCMetricsOptions = *grpcMetrics;
                exportMetrics = true;
                exportMetricsWithHTTP = false;
            }
        }
    }

    std::string applicationName{APPLICATION_NAME};
    CrowOptions crowOptions;
    UserManagementOptions userManagementOptions;
    AQMSDutyReviewBackend::Auth::DatabaseOptions drpDatabaseOptions;
    AQMSDutyReviewBackend::Auth::JSONWebTokenOptions jsonWebTokenOptions;
    AQMSDutyReviewBackend::Database::Credentials aqmsDatabaseCredentials;
    /// The ancillary AQMS machines, from the [AQMSRemote1], [AQMSRemote2],
    /// ... sections.  Empty when the real-time system is the same box as
    /// post-processing, which is a normal deployment and not a problem.
    std::vector<AQMSDutyReviewBackend::Database::Credentials>
        auxiliaryAQMSCredentials;
/*
#ifdef WITH_OPENLDAP
    AQMSDutyReviewBackend::Auth::OpenLDAPOptions openLDAPOptions;
#endif
*/
    AQMSDutyReviewBackend::OTelOptions::HTTPMetrics otelHTTPMetricsOptions;
    AQMSDutyReviewBackend::OTelOptions::HTTPLog otelHTTPLogOptions;
    AQMSDutyReviewBackend::OTelOptions::GRPCMetrics otelGRPCMetricsOptions;
    AQMSDutyReviewBackend::OTelOptions::GRPCLog otelGRPCLogOptions;
    int verbosity{3};
    bool exportLogs{false};
    bool exportLogsWithHTTP{true};
    bool exportMetrics{false};
    bool exportMetricsWithHTTP{true};
};

}
#endif

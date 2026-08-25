#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <cctype>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#ifdef ENABLE_COMPRESSION
#include <crow/compression.h>
#endif
#include "aqmsDutyReviewBackend/auth/jsonWebTokenOptions.hpp"
#ifdef WITH_OPENLDAP
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"
#endif
#include "aqmsDutyReviewBackend/database/credentials.hpp"
#include "otelOptions.hpp"

#define APPLICATION_NAME "aqmsDutyReviewBackend"
namespace
{

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

#ifdef ENABLE_COMPRESSION
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
            options.compression = crow::compression::algorithm::ZLIB;
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
  
        return options;
    }
    std::string bindAddress{"127.0.0.1"};
    std::string serverName{"aqms-drp-server"};
    uint16_t port{8080};
#ifdef ENABLE_COMPRESSION
    crow::compression::algorithm compression{crow::compression::algorithm::GZIP};
#endif
    int nThreads{1};
#ifdef ENABLE_COMPRESSION
    bool useCompression{true};
#else
    bool useCompression{false};
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
#ifdef WITH_OPENLDAP
        // OpenLDAP 
        openLDAPOptions
            = DRP::Auth::OpenLDAPOptions::fromInitializationFile(
                iniFile, "OpenLDAP");
#endif
        // DRP database
        drpDatabaseCredentials
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

        // Crow
        crowOptions
            = CrowOptions::parseInitializationFile(propertyTree, "Crow");

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

             
        // JWT Auth 
        jsonWebTokenOptions
            = DRP::Auth::JSONWebTokenOptions::fromInitializationFile(
                iniFile, "Authentication");

    }

    std::string applicationName{APPLICATION_NAME};
    CrowOptions crowOptions;
    AQMSDutyReviewBackend::Auth::JSONWebTokenOptions jsonWebTokenOptions;
    AQMSDutyReviewBackend::Database::Credentials aqmsDatabaseCredentials;
    AQMSDutyReviewBackend::Database::Credentials drpDatabaseCredentials;
#ifdef WITH_OPENLDAP
    AQMSDutyReviewBackend::Auth::OpenLDAPOptions openLDAPOptions;
#endif
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

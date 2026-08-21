#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <filesystem>
#include <stdexcept>
#include <string>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"
#include "aqmsDutyReviewBackend/auth/jsonWebTokenOptions.hpp"
#include "otelOptions.hpp"

#define APPLICATION_NAME "aqmsDutyReviewBackend"
namespace
{

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
        // OpenLDAP 
        openLDAPOptions
            = DRP::Auth::OpenLDAPOptions::fromInitializationFile(
                iniFile, "OpenLDAP");

        // JWT Auth 
        jsonWebTokenOptions
            = DRP::Auth::JSONWebTokenOptions::fromInitializationFile(
                iniFile, "Authentication");

    }

    std::string applicationName{APPLICATION_NAME};
    AQMSDutyReviewBackend::Auth::JSONWebTokenOptions jsonWebTokenOptions;
    AQMSDutyReviewBackend::Auth::OpenLDAPOptions openLDAPOptions;
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

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include <crow.h>
#include "aqmsDutyReviewBackend/version.hpp"
#include "aqmsDutyReviewBackend/auth/openldap.hpp"
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"
#include "programOptions.hpp"
#include "parseCommandLineOptions.hpp"


int main(int argc, char *argv[])
{
    namespace DRP = AQMSDutyReviewBackend;
    //NOLINTNEXTLINE(misc-include-cleaner)
    auto consoleLogger = spdlog::stdout_color_st("console");
    SPDLOG_LOGGER_INFO(consoleLogger,
                       "Running version {} of aqmsDutyReviewBackend",
                       DRP::Version::getVersionWithTag());

    // Parse the command line arguments
    std::filesystem::path iniFile;
    try
    {
        auto [iniFileName, isHelp] = ::parseCommandLineOptions(argc, argv);
        if (isHelp){return EXIT_SUCCESS;}
        if (iniFileName.empty())
        {
            throw std::runtime_error("No initialization file specified");
        }
        iniFile = iniFileName;
    }
    catch (const std::exception &e)
    {
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to read command line options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }

    // Parse that ini file
    ::ProgramOptions programOptions;

    crow::SimpleApp app;
    CROW_ROUTE(app, "/")
    ([&]()  
    {
/*
        SPDLOG_LOGGER_DEBUG(customLogger.logger, "Default route"); 
        crow::response response;
        response.code = 200;
        response.set_header("Content-Type", "application/json");
        response.body = documentAPI().dump();
        return response;//crow::response(200);
*/
        return crow::response(200);
    }); 

    /// Run app
    try
    {
        app.bindaddr("127.0.0.0") //programOptions.crowBindAddress)
           .port(8080) //programOptions.crowPort)
           .server_name("aqms-drp-server") //programOptions.crowServerName)
           .concurrency(1) //programOptions.nThreads)
#ifdef ENABLE_COMPRESSION
           .use_compression(crow::compression::algorithm::GZIP)
#endif
           .multithreaded()
           .run();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {

        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


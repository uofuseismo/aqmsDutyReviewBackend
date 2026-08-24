#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <stdlib.h> // setenv
#include <string>
#include <utility>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include <crow.h>
#include <crow/app.h>
#include <crow/http_response.h>
#include <crow/logging.h>
#include "aqmsDutyReviewBackend/version.hpp"
#include "programOptions.hpp"
#include "parseCommandLineOptions.hpp"
#include "logger.hpp"
#include "metrics.hpp"

class CustomLogger : public crow::ILogHandler
{
public:
    CustomLogger()
    {
        ::ProgramOptions programOptions;
        programOptions.exportLogs = false;
        programOptions.verbosity = 3;
        logger
            = AQMSDutyReviewBackend::Logger::initialize(programOptions);
        if (logger == nullptr)
        {
            throw std::runtime_error(
                "Failed to initialize crow console logger");
        }
    }
    explicit CustomLogger(std::shared_ptr<spdlog::logger> existingLogger) :
        logger(std::move(existingLogger))
    {
        if (logger == nullptr)
        {
            // NOLINTBEGIN(misc-include-cleaner)
             constexpr const char *loggerName{"crow-console"};
            // N.B. stdout_color_mt throws if the name is already registered.
            auto logger = spdlog::get(loggerName);
            if (logger == nullptr)
            {
                logger = spdlog::stdout_color_mt(loggerName);
            }
            // NOLINTEND(misc-include-cleaner)
        }
        if (logger == nullptr)
        {
            throw std::runtime_error("Failed to initialize logger");
        }
    }
    /*
    ~CustomLogger()
    {    
        AQMSDutyReviewBackend::Logger::cleanup();
    }
    */
    void log(const std::string &message,  crow::LogLevel level)
    {    
        if (logger == nullptr){return;}
        // Most common
        if (level == crow::LogLevel::Info)
        {
            logger->info(message);
        }
        else if (level == crow::LogLevel::Warning)
        {
            logger->warn(message);
        }
        else if (level == crow::LogLevel::Critical)
        {
            logger->critical(message);
        }
        else if (level == crow::LogLevel::Error)
        {
            logger->error(message);
        }
        else if (level == crow::LogLevel::Debug)
        {
            logger->debug(message);
        }
        else
        {
            logger->warn("Unhandled log level - logging " + message);
        }
    }    
    std::shared_ptr<spdlog::logger> logger{nullptr};
};

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
    try 
    {   
        programOptions.parseInitializationFile(iniFile);
    }   
    catch (const std::exception &e) 
    {   
        SPDLOG_LOGGER_CRITICAL(consoleLogger,
                               "Failed to read program options because {}",
                               std::string {e.what()});
        return EXIT_FAILURE;
    }
    if (getenv("OTEL_SERVICE_NAME") == nullptr)
    {
        constexpr int overwrite{1};
        setenv("OTEL_SERVICE_NAME",
               programOptions.applicationName.c_str(),
               overwrite);
    }
    // Create the logger (and one for crow)
    std::shared_ptr<spdlog::logger> logger
        = AQMSDutyReviewBackend::Logger::initialize(programOptions);
    const ::CustomLogger customLogger{logger};

    crow::SimpleApp app;
    CROW_ROUTE(app, "/")
    ([&]()  
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger, "Default route"); 
/*
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
        app.bindaddr("127.0.0.1") //programOptions.crowBindAddress)
           .port(8080) //programOptions.crowPort)
           .server_name("aqms-drp-server") //programOptions.crowServerName)
           .concurrency(1) //programOptions.nThreads)
#ifdef ENABLE_COMPRESSION
           .use_compression(crow::compression::algorithm::GZIP)
#endif
           .multithreaded()
           .run();
        AQMSDutyReviewBackend::Metrics::cleanup();
        AQMSDutyReviewBackend::Logger::cleanup();
        return EXIT_SUCCESS;
    }
    catch (const std::exception &e)
    {
        AQMSDutyReviewBackend::Metrics::cleanup();
        AQMSDutyReviewBackend::Logger::cleanup();
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}


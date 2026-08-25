#include <cstdlib>
#include <cstdint>
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
#include <crow/json.h>
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
    ~CustomLogger()
    {
        logger = nullptr;
    }
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
    ::CustomLogger customLogger{logger};

    crow::logger::setHandler(&customLogger);
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
    CROW_ROUTE(app, "/settings")
    ([&](const crow::request &request)
    {
        // TODO must authorize user's jwt
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Processing version request");
        crow::response response;
        response.code = 200;
        response.set_header("Content-Type", "application/json");
        auto version = AQMSDutyReviewBackend::Version::getVersion();
        crow::json::wvalue wSettings;
        wSettings["backendVersion"] = version;
        wSettings["stadiaMapKey"] = "super-secret-map-key"; 
        response.body = wSettings.dump();
        return response;
    });
    ///----------------------------------------------------------------------///
    ///                               Login Stuff                            ///
    ///----------------------------------------------------------------------///
    CROW_ROUTE(app, "/auth/login")
    ([&](const crow::request &request)
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Login");
        auto authorizationString
            = request.get_header_value("Authorization");
 

        return crow::response(200);
    });
    ///----------------------------------------------------------------------///
    ///                                 Queries                              ///
    ///----------------------------------------------------------------------///
    CROW_ROUTE(app, "/event-information/locks")
    ([&](const crow::request &request)
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Getting database locks...");
        return crow::response(200);
    });
    CROW_ROUTE(app, "/event-information/catalog")
    ([&](const crow::request &request)
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Getting latest catalog...");
        return crow::response(200);
    });
    CROW_ROUTE(app, "/event-information/catalog-hash")
    ([&]()
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Getting latest catalog hash...");
        return crow::response(200);
    });
    ///----------------------------------------------------------------------///
    ///                                 Actions                              ///
    ///----------------------------------------------------------------------///
    CROW_ROUTE(app, "/actions/accept/<int>")
    ([&](int64_t eventIdentifier)
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Accepting... {}", eventIdentifier);
        return crow::response(200);
    });
    CROW_ROUTE(app, "/actions/cancel/<int>")
    ([&](int64_t eventIdentifier)
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Canceling... {}", eventIdentifier);
        return crow::response(200);
    });

    /// Run app
    try
    {
        SPDLOG_LOGGER_INFO(customLogger.logger,
                           "Starting Crow on {}:{}",
                           programOptions.crowOptions.bindAddress,
                           programOptions.crowOptions.port);
        app.bindaddr(programOptions.crowOptions.bindAddress)
           .port(programOptions.crowOptions.port)
           .server_name(programOptions.crowOptions.serverName);
        if (programOptions.crowOptions.nThreads > 1)
        {
           app.concurrency(programOptions.crowOptions.nThreads);
           app.multithreaded();
        }
#ifdef ENABLE_COMPRESSION
        if (programOptions.crowOptions.useCompression)
        {
            .use_compression(programOptions.crowOptions.compression);
        }
#endif
        app.run();
        SPDLOG_LOGGER_INFO(consoleLogger, "Finished running Crow");
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


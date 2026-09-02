#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>
#include <stdlib.h> // setenv
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
//#include <boost/algorithm/string/trim.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <crow/app.h>
#include <crow/http_request.h>
#include <crow/http_response.h>
#include <crow/logging.h>
#include "aqmsDutyReviewBackend/auth/database.hpp"
#include "aqmsDutyReviewBackend/auth/jsonWebToken.hpp"
#include "aqmsDutyReviewBackend/auth/authNZ.hpp"
#include "aqmsDutyReviewBackend/database/client.hpp"
#include "aqmsDutyReviewBackend/database/aqms/database.hpp"
#include "aqmsDutyReviewBackend/database/aqms/serialize.hpp"
//#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
//#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/database/drp/serialize.hpp"
//#include "aqmsDutyReviewBackend/database/drp/userStore.hpp"
//#include "aqmsDutyReviewBackend/auth/password.hpp"
//#include "aqmsDutyReviewBackend/hash.hpp"
#include "aqmsDutyReviewBackend/metricsSingleton.hpp"
#include "aqmsDutyReviewBackend/version.hpp"
#include "authorizeRoute.hpp"
#include "requestBody.hpp"
#include "routes/actionRoutes.hpp"
#include "routes/adminRoutes.hpp"
#include "routes/eventRoutes.hpp"
#include "routes/routeContext.hpp"
#include "routes/stationRoutes.hpp"
#include "routes/waveformRoutes.hpp"
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
    DRP::Metrics::initializeMetricsSingleton();
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

    // Initialize the main AQMS database connection.  Persistent: this one
    // is talked to constantly, and dialling here means bad credentials or
    // an unreachable host are found at startup rather than on the first
    // request.
    auto aqmsClient
        = std::make_shared<AQMSDutyReviewBackend::Database::Client>
          (programOptions.aqmsDatabaseCredentials, logger,
           AQMSDutyReviewBackend::Database::Client::ConnectionPolicy::Persistent);

    // And the ancillary AQMS databases (if they were specified).  OnDemand:
    // these are touched a few times a week, for alarms, and one of them
    // being down must not stop this application from starting.
    std::vector<std::shared_ptr<AQMSDutyReviewBackend::Database::Client>>
        auxiliaryAQMSClients;
    auxiliaryAQMSClients.reserve(
        programOptions.auxiliaryAQMSCredentials.size());
    for (const auto &credentials : programOptions.auxiliaryAQMSCredentials)
    {
        auxiliaryAQMSClients.push_back(
            std::make_shared<AQMSDutyReviewBackend::Database::Client>
            (credentials, logger,
             AQMSDutyReviewBackend::Database::Client::ConnectionPolicy::OnDemand));
    }

    auto aqmsDatabase
        = std::make_unique<AQMSDutyReviewBackend::Database::AQMS::Database>
          (aqmsClient, auxiliaryAQMSClients, logger);

    // Initialize the backend database.  The store is built out here, not
    // left inside the authenticator, because the user-management routes
    // talk to it directly without going through authentication.
    auto drpClient
        = std::make_shared<AQMSDutyReviewBackend::Database::Client>
          (programOptions.drpDatabaseOptions.getCredentials(), logger,
           AQMSDutyReviewBackend::Database::Client::ConnectionPolicy::Persistent);
    auto users
        = std::make_shared<AQMSDutyReviewBackend::Database::DRP::UserStore>
          (drpClient, logger);
    auto drpDatabase
        = std::make_unique<AQMSDutyReviewBackend::Auth::Database> (users,
                                                                   logger);

    // Initialize the JWT
    auto jwtAuthenticator
        = std::make_unique<AQMSDutyReviewBackend::Auth::JSONWebToken>
          (programOptions.jsonWebTokenOptions, logger);

    // Create the authenticator
    auto authenticator
        = std::make_unique<AQMSDutyReviewBackend::Auth::AuthNZ>
          (std::move(drpDatabase), std::move(jwtAuthenticator), logger);


    // How long a provisional password stays usable.  Site policy, so it
    // comes from the [UserManagement] section rather than being compiled
    // in here.
    const std::chrono::seconds newAccountLifetime
        {programOptions.userManagementOptions.provisionalAccountExpiresAfter};
    const std::chrono::seconds passwordResetLifetime
        {programOptions.userManagementOptions.passwordResetExpiresAfter};

    /// @brief Turns an AdminResult into the response it means.
    /// @note The database distinguishes "you may not" from "that did not
    ///       work" on purpose, and this is where that distinction becomes
    ///       a 403 rather than a 400.  Collapsing them would tell an
    ///       administrator their input was wrong when it was not.
    const auto adminResponse
        = [](const AQMSDutyReviewBackend::Database::DRP::AdminResult result,
             const std::string &succeeded,
             const std::string &failed) -> crow::response
    {
        using AdminResult = AQMSDutyReviewBackend::Database::DRP::AdminResult;
        if (result == AdminResult::Succeeded)
        {
            return ::makeMessageResponse(200, succeeded);
        }
        if (result == AdminResult::NotAuthorized)
        {
            // The database refused the actor - not an administrator, not
            // activated yet, or this would have removed the last one.
            return ::makeMessageResponse(
                403, "You are not permitted to do that");
        }
        return ::makeMessageResponse(400, failed);
    };

    /// @brief Fetches the stations and serializes them.
    /// @result The JSON, or nullopt if AQMS could not be reached.
    /// @note Shared by /station-information and /station-information-hash
    ///       so the two cannot disagree: a hash computed over a different
    ///       serialization than the body would have clients re-downloading
    ///       forever, or worse, never.
    /// @note This is the seam the cache goes behind later.  The poller
    ///       prefetches, this asks the cache and falls through to the
    ///       database on a miss or a forced refresh - and neither route
    ///       above it changes.
    const auto fetchStationsJSON
        = [&aqmsDatabase]() -> std::optional<boost::json::value>
    {
        const auto stations = aqmsDatabase->fetchStations();
        if (!stations){return std::nullopt;}
        return AQMSDutyReviewBackend::Database::AQMS::toJSON(*stations);
    };

    // Everything the route handlers share, assembled once.  The two raw
    // pointers are not owned - main owns those objects and outlives
    // app.run(), which is what makes it safe.
    const ::RouteContext routeContext
    {
        authenticator.get(),
        aqmsDatabase.get(),
        users,
        logger,
        std::chrono::seconds
            {programOptions.userManagementOptions.provisionalAccountExpiresAfter},
        std::chrono::seconds
            {programOptions.userManagementOptions.passwordResetExpiresAfter}
    };

    crow::logger::setHandler(&customLogger);
    crow::SimpleApp app;
    // The default route stays here: it is the only one that is neither
    // authorized nor part of a family.
    CROW_ROUTE(app, "/")
    ([&]() -> crow::response
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger, "Default route");
        return crow::response(200);
    });

    CROW_ROUTE(app, "/settings")
    ([&](const crow::request &request) -> crow::response
    {
        // TODO must authorize user's jwt
        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "Processing settings request");
        boost::json::object settings;
        settings["backendVersion"]
            = AQMSDutyReviewBackend::Version::getVersion();
        settings["stadiaMapKey"] = programOptions.stadiaMapsAPIKey;
        auto primaryDatabase
            = programOptions.aqmsDatabaseCredentials.getAlias();
        if (primaryDatabase)
        {
            settings["primaryDatabase"]
                = primaryDatabase->empty() ? "Unknown" : *primaryDatabase;
        }
        else
        {
            settings["primaryDatabase"] = "Unknown";
        }
        return ::makeDataResponse(200, "Settings", std::move(settings));
    });

    // Login is its own shape: it turns a password into a token, so it
    // cannot go through the authorization helper that expects one.
    CROW_ROUTE(app, "/auth/login")
    ([&](const crow::request &request) -> crow::response
    {
        return ::userLoginRoute(request, *authenticator, logger);
    });

    // Everything else is grouped by what it is about.  Adding a route
    // means editing one of these files, not this one.
    ::registerStationRoutes(app, routeContext);
    ::registerEventRoutes(app, routeContext);
    ::registerActionRoutes(app, routeContext);
    ::registerAdminRoutes(app, routeContext);
    ::registerWaveformRoutes(app, routeContext);

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
#ifdef CROW_ENABLE_COMPRESSION
        if (programOptions.crowOptions.useCompression)
        {
            SPDLOG_LOGGER_INFO(consoleLogger, "Enabling compression");
            app.use_compression(programOptions.crowOptions.compression);
        }
#endif
#ifdef CROW_ENABLE_SSL
        if (programOptions.crowOptions.useSSL)
        {
            if (programOptions.crowOptions.useCertificateChain)
            {
                SPDLOG_LOGGER_INFO(consoleLogger, "Enabling SSL key chain");
                app.ssl_chainfile(
                   programOptions.crowOptions.certificateAndKeyFile.first, 
                   programOptions.crowOptions.certificateAndKeyFile.second);
            }
            else
            {
                SPDLOG_LOGGER_INFO(consoleLogger, "Enabling SSL keys");
                app.ssl_file(
                   programOptions.crowOptions.certificateAndKeyFile.first, 
                   programOptions.crowOptions.certificateAndKeyFile.second);
            }
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


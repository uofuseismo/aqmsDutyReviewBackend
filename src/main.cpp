#include <cstdlib>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <stdlib.h> // setenv
#include <string>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/stdout_color_sinks.h> //NOLINT
#include <boost/algorithm/string/trim.hpp>
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
#include "aqmsDutyReviewBackend/database/aqms/eventLock.hpp"
#include "aqmsDutyReviewBackend/database/aqms/station.hpp"
#include "aqmsDutyReviewBackend/database/drp/serialize.hpp"
#include "aqmsDutyReviewBackend/database/drp/userStore.hpp"
#include "aqmsDutyReviewBackend/auth/password.hpp"
#include "aqmsDutyReviewBackend/hash.hpp"
#include "aqmsDutyReviewBackend/metricsSingleton.hpp"
#include "aqmsDutyReviewBackend/version.hpp"
#include "authorizeRoute.hpp"
#include "requestBody.hpp"
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
                            "Processing settings request");
        boost::json::object settings;
        settings["backendVersion"]
            = AQMSDutyReviewBackend::Version::getVersion();
        settings["stadiaMapKey"] = "super-secret-map-key";
        return ::makeDataResponse(200, "Settings", std::move(settings));
    });
    ///----------------------------------------------------------------------///
    ///                               Login Stuff                            ///
    ///----------------------------------------------------------------------///
    CROW_ROUTE(app, "/auth/login")
    ([&](const crow::request &request)
    {
        SPDLOG_LOGGER_DEBUG(customLogger.logger, "Login route");
        try
        {
            return ::userLoginRoute(request, *authenticator, logger);
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(logger, "User login failed because {}",
                                e.what());
            return ::makeMessageResponse(
                500, "Server error - contact developer");
        }
    });
    ///----------------------------------------------------------------------///
    ///                                 Queries                              ///
    ///----------------------------------------------------------------------///
    CROW_ROUTE(app, "/station-information")
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_INFO(customLogger.logger,
                           "Getting stations for {}",
                           authResult.identity->user);

        const auto stations = fetchStationsJSON();
        if (!stations)
        {
            // The query takes no arguments, so the only way it fails is
            // AQMS being unreachable - this backend's problem to report,
            // not something the caller can fix by asking differently.
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not fetch stations for {}",
                                authResult.identity->user);
            return ::makeMessageResponse(
                500, "Could not reach the AQMS database - try again shortly");
        }
        return ::makeDataResponse(
            200,
            "Found " + std::to_string(stations->as_array().size())
                     + " station epochs",
            *stations);
    });
    CROW_ROUTE(app, "/event-information/locks")
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_INFO(customLogger.logger,
                           "Getting database locks for {}",
                           authResult.identity->user);

        // Never cached: a lock's whole purpose is to say who is working an
        // event right now, and a cached answer would be exactly the stale
        // one that puts two analysts on the same event.
        const auto locks = aqmsDatabase->getLockedEvents();
        if (!locks)
        {
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not fetch event locks for {}",
                                authResult.identity->user);
            return ::makeMessageResponse(
                500, "Could not reach the AQMS database - try again shortly");
        }
        return ::makeDataResponse(
            200,
            std::to_string(locks->size()) + " locked event(s)",
            AQMSDutyReviewBackend::Database::AQMS::toJSON(*locks));
    });
    CROW_ROUTE(app, "/event-information/catalog")
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "{} requesting catalog...",
                            authResult.identity->user);

        return crow::response(200);
    });
    CROW_ROUTE(app, "/event-information/catalog-hash")
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "{} requesting catalog hash...",
                            authResult.identity->user);

        return crow::response(200);
    });
    CROW_ROUTE(app, "/waveforms/<int>")
    ([&](const crow::request &request, const int64_t eventIdentifier)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "{} requesting waveforms for {}...",
                            authResult.identity->user,
                            eventIdentifier);

        return crow::response(200);
    }); 
    CROW_ROUTE(app, "/waveforms-hash/<int>")
    ([&](const crow::request &request, const int64_t eventIdentifier)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "{} requesting waveforms hash for {}...",
                            authResult.identity->user,
                            eventIdentifier);

        return crow::response(200);
    });
    CROW_ROUTE(app, "/station-information-hash")
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadOnly,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_DEBUG(customLogger.logger,
                            "{} requesting stations hash...",
                            authResult.identity->user);

        const auto stations = fetchStationsJSON();
        if (!stations)
        {
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not fetch stations for {}",
                                authResult.identity->user);
            return ::makeMessageResponse(
                500, "Could not reach the AQMS database - try again shortly");
        }
        // Hashed over the same serialization the body route sends, which
        // is why both go through fetchStationsJSON.  A hash taken over
        // anything else would either never change or never match.
        boost::json::object payload;
        payload["hash"]
            = AQMSDutyReviewBackend::hash(boost::json::serialize(*stations));
        return ::makeDataResponse(200, "Station information hash",
                                  std::move(payload));
    });


    ///----------------------------------------------------------------------///
    ///                                 Actions                              ///
    ///----------------------------------------------------------------------///
    CROW_ROUTE(app, "/actions/accept/<int>")
    ([&](const crow::request &request, int64_t eventIdentifier)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadWrite,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_INFO(customLogger.logger,
                           "{} accepting event {}",
                           authResult.identity->user,
                           eventIdentifier);

        return crow::response(200);
    });
    CROW_ROUTE(app, "/actions/cancel/<int>")
    ([&](const crow::request &request, int64_t eventIdentifier)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::ReadWrite,
            false // Require password
        };                 
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_INFO(customLogger.logger,
                           "{} canceling event {}",
                           authResult.identity->user,
                           eventIdentifier);

        return crow::response(200);
    });
    ///----------------------------------------------------------------------///
    ///                            Administration Stuff                      ///
    ///----------------------------------------------------------------------///
    ///----------------------------------------------------------------------///
    ///                            User management                           ///
    ///----------------------------------------------------------------------///
    /// Every one of these takes the acting administrator from their token
    /// and hands it to the database, which checks the authority itself.
    /// The Administrator requirement below says who may ASK; the actor
    /// argument says on whose behalf.  Both have to hold, and the database
    /// is the one that decides.
    CROW_ROUTE(app, "/actions/admin/add-provisional-user")
    .methods("POST"_method)
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::Administrator,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request, *authenticator,
                                           requirement, logger);
        if (!authResult){return std::move(*authResult.rejection);}

        auto body = ::parseRequestData(request);
        if (!body){return std::move(*body.rejection);}

        const auto user = ::requiredString(*body.data, "user");
        if (!user){return ::missingField("user");}
        const auto permissionText = ::requiredString(*body.data, "permission");
        if (!permissionText){return ::missingField("permission");}

        const auto permission
            = AQMSDutyReviewBackend::Auth::IAuthenticator::stringToPermissions(
                  *permissionText);
        if (permission
            == AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::None)
        {
            // stringToPermissions answers None for anything it does not
            // recognise, so a typo denies rather than granting something.
            return ::makeMessageResponse(
                400, "\"data.permission\" must be read_only, read_write, "
                     "or admin");
        }

        // The caller may name the temporary password; if it does not, one
        // is generated.  Generated is better - a distinct random password
        // per account is exactly what stops a shared "changeme" spreading
        // across every unactivated account - so the response returns
        // whichever was used and the administrator passes it on.
        auto temporaryPassword = ::requiredString(*body.data,
                                                  "temporaryPassword");
        if (!temporaryPassword)
        {
            temporaryPassword
                = AQMSDutyReviewBackend::Auth::generateTemporaryPassword();
        }

        try
        {
            const auto result = users->addProvisionalUser(
                authResult.identity->user, *user,
                AQMSDutyReviewBackend::Auth::hashPassword(*temporaryPassword),
                newAccountLifetime,
                AQMSDutyReviewBackend::Auth::IAuthenticator
                    ::permissionsToString(permission));
            if (result
                != AQMSDutyReviewBackend::Database::DRP::AdminResult::Succeeded)
            {
                return adminResponse(result, "", "Could not add " + *user
                                   + " - the name may already be taken");
            }
            SPDLOG_LOGGER_INFO(customLogger.logger, "{} added {}",
                               authResult.identity->user, *user);
            // The password is in the body and not the log, because the log
            // outlives the credential and this is the one place it legibly
            // exists.
            boost::json::object data;
            data["user"] = *user;
            data["temporaryPassword"] = *temporaryPassword;
            return ::makeDataResponse(
                200,
                "Added " + *user + ".  Give them this password out of band; "
                "it expires and must be changed on first login.",
                std::move(data));
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not add {} because {}",
                                *user, std::string {e.what()});
            return ::makeMessageResponse(500, "Could not add the user");
        }
    });

    CROW_ROUTE(app, "/actions/admin/reset-user-password")
    .methods("POST"_method)
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::Administrator,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request, *authenticator,
                                           requirement, logger);
        if (!authResult){return std::move(*authResult.rejection);}

        auto body = ::parseRequestData(request);
        if (!body){return std::move(*body.rejection);}
        const auto user = ::requiredString(*body.data, "user");
        if (!user){return ::missingField("user");}

        // Always generated, never chosen: a reset hands back a credential
        // the administrator is holding, so it had better be one that
        // expires and that they did not pick.
        const auto temporaryPassword
            = AQMSDutyReviewBackend::Auth::generateTemporaryPassword();
        try
        {
            const auto result = users->resetUserPassword(
                authResult.identity->user, *user,
                AQMSDutyReviewBackend::Auth::hashPassword(temporaryPassword),
                passwordResetLifetime);
            if (result
                != AQMSDutyReviewBackend::Database::DRP::AdminResult::Succeeded)
            {
                return adminResponse(result, "",
                                     "Could not reset " + *user
                                   + " - no such user");
            }
            SPDLOG_LOGGER_INFO(customLogger.logger, "{} reset {}",
                               authResult.identity->user, *user);
            boost::json::object data;
            data["user"] = *user;
            data["temporaryPassword"] = temporaryPassword;
            return ::makeDataResponse(
                200,
                "Reset " + *user + ".  Give them this password out of band; "
                "it expires and must be changed on their next login.",
                std::move(data));
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not reset {} because {}",
                                *user, std::string {e.what()});
            return ::makeMessageResponse(500, "Could not reset the password");
        }
    });

    CROW_ROUTE(app, "/actions/admin/set-user-permission")
    .methods("POST"_method)
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::Administrator,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request, *authenticator,
                                           requirement, logger);
        if (!authResult){return std::move(*authResult.rejection);}

        auto body = ::parseRequestData(request);
        if (!body){return std::move(*body.rejection);}
        const auto user = ::requiredString(*body.data, "user");
        if (!user){return ::missingField("user");}
        const auto permissionText = ::requiredString(*body.data, "permission");
        if (!permissionText){return ::missingField("permission");}

        const auto permission
            = AQMSDutyReviewBackend::Auth::IAuthenticator::stringToPermissions(
                  *permissionText);
        if (permission
            == AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::None)
        {
            return ::makeMessageResponse(
                400, "\"data.permission\" must be read_only, read_write, "
                     "or admin");
        }

        try
        {
            const auto result = users->setUserPermission(
                authResult.identity->user, *user,
                AQMSDutyReviewBackend::Auth::IAuthenticator
                    ::permissionsToString(permission));
            SPDLOG_LOGGER_INFO(customLogger.logger,
                               "{} set {} to {}", authResult.identity->user,
                               *user, *permissionText);
            // Demoting the last administrator comes back NotAuthorized -
            // the database refuses to leave itself with nobody who can
            // administer it - so that lands as a 403, not a 400.
            return adminResponse(result,
                                 "Set " + *user + " to " + *permissionText,
                                 "Could not change " + *user
                               + " - no such user");
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not change {} because {}",
                                *user, std::string {e.what()});
            return ::makeMessageResponse(500,
                                         "Could not change the permission");
        }
    });

    CROW_ROUTE(app, "/actions/admin/remove-user")
    .methods("POST"_method)
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::Administrator,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request, *authenticator,
                                           requirement, logger);
        if (!authResult){return std::move(*authResult.rejection);}

        auto body = ::parseRequestData(request);
        if (!body){return std::move(*body.rejection);}
        const auto user = ::requiredString(*body.data, "user");
        if (!user){return ::missingField("user");}

        try
        {
            const auto result
                = users->removeUser(authResult.identity->user, *user);
            SPDLOG_LOGGER_INFO(customLogger.logger, "{} removed {}",
                               authResult.identity->user, *user);
            // Their keys go with them, and removing the last administrator
            // is refused by the database rather than permitted.
            return adminResponse(result, "Removed " + *user,
                                 "Could not remove " + *user
                               + " - no such user");
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not remove {} because {}",
                                *user, std::string {e.what()});
            return ::makeMessageResponse(500, "Could not remove the user");
        }
    });

    CROW_ROUTE(app, "/actions/admin/list-users")
    ([&](const crow::request &request)
    {
        constexpr AQMSDutyReviewBackend::Auth::Requirement requirement
        {
            AQMSDutyReviewBackend::Auth::IAuthenticator::Permissions::Administrator,
            false // Require password
        };
        auto authResult = ::authorizeRoute(request,
                                           *authenticator,
                                           requirement,
                                           logger);
        if (!authResult){return std::move(*authResult.rejection);}

        SPDLOG_LOGGER_INFO(customLogger.logger,
                           "Listing users for {}",
                           authResult.identity->user);

        // Straight to the store - this is the backend's own database, not
        // AQMS, and it never goes through the authenticator.
        try
        {
            auto userList = users->listUsers();
            return ::makeDataResponse(
                200,
                std::to_string(userList.size()) + " user(s)",
                AQMSDutyReviewBackend::Database::DRP::toJSON(userList));
        }
        catch (const std::exception &e)
        {
            SPDLOG_LOGGER_ERROR(customLogger.logger,
                                "Could not list users for {} because {}",
                                authResult.identity->user,
                                std::string {e.what()});
            return ::makeMessageResponse(500, "Could not list the users");
        }
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


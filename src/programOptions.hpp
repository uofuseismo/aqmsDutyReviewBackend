#ifndef PROGRAM_OPTIONS_HPP
#define PROGRAM_OPTIONS_HPP
#include <string>
#include "aqmsDutyReviewBackend/auth/openldapOptions.hpp"
#include "aqmsDutyReviewBackend/auth/jsonWebTokenOptions.hpp"

#define APPLICATION_NAME "aqmsDutyReviewBackend"
namespace
{

struct ProgramOptions
{
    std::string applicationName{APPLICATION_NAME};
    AQMSDutyReviewBackend::Auth::JSONWebTokenOptions jsonWebTokenOptions;
    AQMSDutyReviewBackend::Auth::OpenLDAPOptions openLDAPOptions;
};

}
#endif

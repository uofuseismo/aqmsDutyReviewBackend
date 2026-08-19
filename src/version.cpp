#include <string>
#include "aqmsDutyReviewBackend/version.hpp"

using namespace AQMSDutyReviewBackend;

int Version::getMajor() noexcept
{
    return aqmsDutyReviewBackend_MAJOR;
}

int Version::getMinor() noexcept
{
    return aqmsDutyReviewBackend_MINOR;
}

int Version::getPatch() noexcept
{
    return aqmsDutyReviewBackend_PATCH;
}

//NOLINTBEGIN(bugprone-easily-swappable-parameters)
bool Version::isAtLeast(const int major, const int minor,
                        const int patch) noexcept
//NOLINTEND(bugprone-easily-swappable-parameters)
{
    if (aqmsDutyReviewBackend_MAJOR < major){return false;}
    if (aqmsDutyReviewBackend_MAJOR > major){return true;}
    if (aqmsDutyReviewBackend_MINOR < minor){return false;}
    if (aqmsDutyReviewBackend_MINOR > minor){return true;}
    if (aqmsDutyReviewBackend_PATCH < patch){return false;}
    return true;
}

std::string Version::getVersion() noexcept
{
    std::string version{aqmsDutyReviewBackend_VERSION};
    return version;
}

std::string Version::getTag() noexcept
{
    std::string tag{aqmsDutyReviewBackend_GITTAG};
    return tag;
}

std::string Version::getVersionWithTag() noexcept
{
    auto tag = Version::getTag();
    if (tag.empty())
    {
        return Version::getVersion();
    }
    else
    {
        return Version::getVersion() + "-" + tag;
    }
}

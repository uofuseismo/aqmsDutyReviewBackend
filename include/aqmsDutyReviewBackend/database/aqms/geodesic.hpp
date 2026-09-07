#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_GEODESIC_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_GEODESIC_HPP
#include <memory>
#include <optional>
namespace AQMSDutyReviewBackend::Database::AQMS
{
 class Origin;
 class Station;
}

namespace AQMSDutyReviewBackend::Database::AQMS::Geodesic
{

/// @class DistanceAzimuth distanceAzimuth.hpp
/// @brief A utility to hold the result of the geodesic calculations on a WGS84
///        earth.  This is a last ditch effort utility to get a distance and
///        azimuth to an output.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class DistanceAzimuth
{
public:
    /// @brief Constructor.
    DistanceAzimuth();
    /// @brief Copy assignment.
    DistanceAzimuth(const DistanceAzimuth &distanceAzimuth);
    /// @brief Move assignment.
    DistanceAzimuth(DistanceAzimuth &&distanceAzimuth) noexcept;
    /// @throws std::invalid_argument if origin.hasLatitude() or
    ///         origin.hasLongitude() is false or
    ///         station.hasLatitude() or station.hasLongitude() is false.
    DistanceAzimuth(const Origin &origin, const Station &station);
   
    /// @brief The source-to-receiver distance in kilometers. 
    /// @throws std::invalid_argument if this is negative.
    void setDistance(double distance);
    /// @result The source-to-receiver distance in kilometers.
    [[nodiscard]] std::optional<double> getDistance() const noexcept;
    /// @result True indicates the distance was set.
    /// @brief The source-to-receiver azimuth measured positive east of
    ///        north.  
    /// @throws std::invalid_argument if this is not in the
    ///         range [0, 360) degrees.
    void setAzimuth(double azimuth);
    /// @result The source-to-receiver azimuth in degrees.
    [[nodiscard]] std::optional<double> getAzimuth() const noexcept;
    /// @brief The reeiver-to-source azimuth measured positive east of
    ///         north.  This is in the range [0, 360) degrees.
    /// @throws std::invalid_argument if this is not in the
    ///         range [0, 360) degrees.
    void setBackAzimuth(double backAzimuth);
    /// @result The receiver-to-source azimuth in degrees.
    [[nodiscard]] std::optional<double> getBackAzimuth() const noexcept;

    /// @brief Ddstructor.
    ~DistanceAzimuth();
    /// @brief Copy assignment.
    DistanceAzimuth &operator=(const DistanceAzimuth &distanceAzimuth);
    /// @brief Move assignment.
    DistanceAzimuth &operator=(DistanceAzimuth &&distanceAzimuth) noexcept;
private:
    class DistanceAzimuthImpl;
    std::unique_ptr<DistanceAzimuthImpl> pImpl;
};
}
#endif

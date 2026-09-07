#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_STATION_LOCAL_MAGNITUDE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_STATION_LOCAL_MAGNITUDE_HPP
#include <chrono>
#include <memory>
#include <optional>
#include <utility>
namespace AQMSDutyReviewBackend::Database::AQMS
{
 class StreamIdentifier;   
 class PeakToPeakAmplitude;
}
namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class StationLocalMagnitude stationLocalMagnitude.hpp
/// @brief Defines a local (Richter) magnitude made by an individual stream.
///        To compute a local magnitude we use something like:
///          M_L = \log_{10} (A) + C_d + C_s
///        where A is a (half) amplitude from a WoodAnderson seismometer,
///        C_d is a distance correction and C_s is a station correction.
///        So for example, if you take \c getAmplitude() on TCU HHN
///        and get an amplitude of 6.434 mm and take \c getAmplitude()
///        on TCU HHE and get 3.180 mm.  Then, at Utah, we compute an
///        average of the channels to get the station magnitude - i.e.
///        two channels make one A by averaging half amplitudes ala
///           A = (6.434/2 + 3.180/2)/2
///        A distance correction at, say, 177 km from a lookup table produces
///           C_d = 3.4
///        And the station's correction is 
///           C_s =-0.55
///        Put it all together and then the station magnitude from the two
///        channels is:
///           log10( (6.434/2 + 3.180/2)/2 ) + 3.4 + (-0.55) = 3.23 
///        If the network magnitude is 3.45 then the residual is
///        3.45 - 3.23 = 0.22
///           
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI License.
class StationLocalMagnitude
{
public:
    /// @brief Constructor.
    StationLocalMagnitude();
    /// @brief Copy constructor.
    StationLocalMagnitude(const StationLocalMagnitude &magnitude); 
    /// @brief Move constructor.
    StationLocalMagnitude(StationLocalMagnitude &&magnitude) noexcept;

    /// @brief Sets the amplitude measurements made on two channels.
    /// @throws std::invalid_argument if the amplitudes are from the same
    ///         stream, do not have peak times, or amplitude values.
    void setPeakToPeakAmplitudes(const std::pair<PeakToPeakAmplitude, PeakToPeakAmplitude> &amplitudes);
    /// @result The amplitude measurements made on two channels.
    /// @throws std::runtime_error if \c hasPeakToPeakAmplitudes() is false.
    [[nodiscard]] std::pair<PeakToPeakAmplitude, PeakToPeakAmplitude> getPeakToPeakAmplitudes() const;
    /// @result True indicates the channel amplitudes were set.
    [[nodiscard]] bool hasPeakToPeakAmplitudes() const noexcept;

    /// @brief Sets this CHANNEL's magnitude - assocamm.mag.
    /// @note Per channel, not per station.  A local magnitude is measured
    ///       on two horizontals - sometimes four channels - and on an
    ///       automatic magnitude those components disagree: they match on
    ///       only 2.8% of stations, with spreads up to 3.6 magnitude
    ///       units.  There is no single station value in AQMS to read, so
    ///       this is what AQMS actually has and the caller is not misled
    ///       into thinking otherwise.
    ///
    ///       Codas differ - one channel per station - which is why
    ///       StationDurationMagnitude's magnitude really is a station
    ///       magnitude and this one is not.
    void setMagnitude(double magnitude) noexcept;
    /// @result This channel's magnitude.
    /// @throws std::runtime_error if \c hasMagnitude() is false.
    [[nodiscard]] double getMagnitude() const;
    /// @result True indicates the magnitude was set.
    [[nodiscard]] bool hasMagnitude() const noexcept;

    /// @brief Sets the residual - this channel's magnitude less the
    ///        network magnitude.
    /// @note assocamm.magres when AQMS supplied one, and otherwise that
    ///       same subtraction.  Not an approximation: magres equals it
    ///       exactly on every reviewed row in the archive.  AQMS writes one
    ///       only on review, so an automatic magnitude has none.
    void setResidual(double residual) noexcept;
    /// @result The residual.
    /// @throws std::runtime_error if \c hasResidual() is false.
    [[nodiscard]] double getResidual() const;
    /// @result True indicates the residual was set.
    [[nodiscard]] bool hasResidual() const noexcept;

    /// @brief Sets the magnitude correction - assocamm.magcorr.
    void setCorrection(double correction) noexcept;
    /// @result The correction.  Zero by default.
    [[nodiscard]] double getCorrection() const noexcept;

    /// @brief Sets the Wood-Anderson amplitude in MILLIMETRES.
    /// @throws std::invalid_argument if the amplitude is not positive.
    /// @note amp.units is not always 'mm' - the archive holds Wood-Anderson
    ///       amplitudes in both 'mm' and 'cm' - so the reader converts and
    ///       this is always millimetres, which is what the scale takes.
    void setAmplitude(double amplitude);
    /// @result The amplitude in millimetres, if it was set.
    [[nodiscard]] std::optional<double> getAmplitude() const noexcept;

    /// @brief Sets the source-receiver distance in meters.
    /// @throws std::invalid_argument if the distance is negative.
    void setSourceReceiverDistance(double distance);
    /// @result The distance in meters, if it was set.
    [[nodiscard]] std::optional<double>
        getSourceReceiverDistance() const noexcept;

    /// @brief Sets the source-to-receiver azimuth in degrees.
    /// @throws std::invalid_argument if outside [0,360].
    void setSourceReceiverAzimuth(double azimuth);
    /// @result The azimuth in degrees, if it was set.
    [[nodiscard]] std::optional<double>
        getSourceReceiverAzimuth() const noexcept;

    /// @brief Sets the channel this was measured on.
    /// @throws std::invalid_argument if the identifier is incomplete.
    void setStreamIdentifier(const StreamIdentifier &identifier);
    /// @brief Sets the channel this was measured on.
    void setStreamIdentifier(StreamIdentifier &&identifier);
    /// @result The channel.
    /// @throws std::runtime_error if \c hasStreamIdentifier() is false.
    [[nodiscard]] StreamIdentifier getStreamIdentifier() const;
    /// @result True indicates the stream identifier was set.
    [[nodiscard]] bool hasStreamIdentifier() const noexcept;

    /// @brief Sets the weight.  This is typically binary 0 or 1.
    /// @param[in] weight  The weight where 0 is disabled and 1 fully utilized.
    /// @throws std::invalid_argument if this is not in the range of [0, 1].
    void setWeight(double weight);
    /// @result The weight.
    /// @throws std::runtime_error if \c hasWeight() is false.
    [[nodiscard]] double getWeight() const;
    /// @result True indicates the weight was set.
    [[nodiscard]] bool hasWeight() const noexcept;

    /// @brief Destructor.
    ~StationLocalMagnitude();
    /// @brief Copy assignment.
    StationLocalMagnitude& operator=(const StationLocalMagnitude &magnitude);
    /// @brief Move assignment.
    StationLocalMagnitude& operator=(StationLocalMagnitude &&) noexcept;
private:
    class StationLocalMagnitudeImpl;
    std::unique_ptr<StationLocalMagnitudeImpl> pImpl;
};
}
#endif

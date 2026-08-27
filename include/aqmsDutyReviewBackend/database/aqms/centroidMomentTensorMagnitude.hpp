#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_CENTROID_MOMENT_TENSOR_MAGNITUDE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_CENTROID_MOMENT_TENSOR_MAGNITUDE_HPP
#include <memory>
#include <aqmsDutyReviewBackend/database/aqms/magnitude.hpp>

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class CentroidMomentTensorMagnitude centroidMomentTensorMagnitude.hpp
/// @brief This is a trimmed down CMT (Mw) magnitude.  Since these are computed
///        out of band from AQMS I'll just cut to the punchline - the Mw value.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class CentroidMomentTensorMagnitude final : public IMagnitude
{
public:
    /// @brief Constructor.
    CentroidMomentTensorMagnitude();
    /// @brief Copy constructor.
    CentroidMomentTensorMagnitude(const CentroidMomentTensorMagnitude &magnitude); 
    /// @brief Move constructor.
    CentroidMomentTensorMagnitude(CentroidMomentTensorMagnitude &&magnitude) noexcept;

    /// @result A type of moment magnitude.
    [[nodiscard]] IMagnitude::Type getType() const noexcept final;

    /// @result A deep copy of this CMT magnitude.
    [[nodiscard]] std::unique_ptr<IMagnitude> clone() const final;

    /// @result The review status the database supplied, or Human when it
    ///         supplied none.
    /// @note The database owns this, and a set value is reported as-is
    ///       even when it is odd.  This application is a view of what
    ///       AQMS holds rather than an authority on it, so an automatic
    ///       CMT comes back automatic - log the oddity where the row is
    ///       read, do not launder it here.
    ///
    ///       The default covers the ordinary case: nobody auto-computes a
    ///       CMT, so a row that says nothing means a person ran another
    ///       application, decided the answer was good, and wrote the
    ///       values in.
    [[nodiscard]] IMagnitude::ReviewStatus getReviewStatus() const noexcept override final;
    /// @result True, always - the default guarantees there is a status to
    ///         read, so this never has to be checked before reading one.
    [[nodiscard]] bool hasReviewStatus() const noexcept override final;

    /// @brief Destructor.
    virtual ~CentroidMomentTensorMagnitude();
    /// @brief Copy assignment.
    CentroidMomentTensorMagnitude& operator=(const CentroidMomentTensorMagnitude &magnitude);
    /// @brief Move assignment.
    CentroidMomentTensorMagnitude& operator=(CentroidMomentTensorMagnitude &&) noexcept;
private:
    class CentroidMomentTensorMagnitudeImpl;
    std::unique_ptr<CentroidMomentTensorMagnitudeImpl> pImpl;
};
}
#endif

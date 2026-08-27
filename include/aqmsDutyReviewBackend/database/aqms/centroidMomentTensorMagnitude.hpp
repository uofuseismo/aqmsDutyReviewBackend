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

    /// @result These are computed out of band and should always be human reviewed.
    [[nodiscard]] IMagnitude::ReviewStatus getReviewStatus() const noexcept override final;
    /// @result True, always.  Nobody auto-computes a CMT - a person runs
    ///         another application, decides the answer is good, and writes
    ///         the values into the database - so the review status is a
    ///         property of the type rather than something a caller has to
    ///         set.  Saying otherwise would have a caller who guards on
    ///         \c hasReviewStatus() skip a value that is always there.
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

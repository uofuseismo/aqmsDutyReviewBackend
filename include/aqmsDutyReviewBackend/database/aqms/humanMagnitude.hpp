#ifndef AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_HUMAN_MAGNITUDE_HPP
#define AQMS_DUTY_REVIEW_BACKEND_DATABASE_AQMS_HUMAN_MAGNITUDE_HPP
#include <memory>
#include <aqmsDutyReviewBackend/database/aqms/magnitude.hpp>

namespace AQMSDutyReviewBackend::Database::AQMS
{
/// @class HumanMagnitude humanMagnitude.hpp
/// @brief Defines a human assigned magnitude.
/// @copyright Ben Baker (University of Utah) distributed under the
///            MIT NO AI license.
class HumanMagnitude final : public IMagnitude
{
public:
    /// @brief Constructor.
    HumanMagnitude();
    /// @brief Copy constructor.
    HumanMagnitude(const HumanMagnitude &magnitude); 
    /// @brief Move constructor.
    HumanMagnitude(HumanMagnitude &&magnitude) noexcept;

    /// @result A type of human magnitude.
    [[nodiscard]] IMagnitude::Type getType() const noexcept final;

    /// @result A deep copy of this human magnitude.
    [[nodiscard]] std::unique_ptr<IMagnitude> clone() const final;

    /// @result The review status the database supplied, or Human when it
    ///         supplied none.
    /// @note The database owns this, and a set value is reported as-is
    ///       even when it is odd.  This application is a view of what
    ///       AQMS holds rather than an authority on it, so an automatic
    ///       human magnitude comes back automatic - log the oddity where
    ///       the row is read, do not launder it here.
    ///
    ///       The default covers the ordinary case: a human assigned it,
    ///       so absent anything to the contrary it is human reviewed.
    [[nodiscard]] IMagnitude::ReviewStatus getReviewStatus() const noexcept override final;
    /// @result True, always - the default guarantees there is a status to
    ///         read, so this never has to be checked before reading one.
    [[nodiscard]] bool hasReviewStatus() const noexcept override final;

    /// @brief Destructor.
    virtual ~HumanMagnitude();
    /// @brief Copy assignment.
    HumanMagnitude& operator=(const HumanMagnitude &magnitude);
    /// @brief Move assignment.
    HumanMagnitude& operator=(HumanMagnitude &&) noexcept;
private:
    class HumanMagnitudeImpl;
    std::unique_ptr<HumanMagnitudeImpl> pImpl;
};
}
#endif

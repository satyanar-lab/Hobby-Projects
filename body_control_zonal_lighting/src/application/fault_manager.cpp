#include "body_control/lighting/application/fault_manager.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

FaultManager::FaultManager() noexcept
    : faulted_ {}
    , active_dtcs_ {}
    , active_fault_count_ {0U}
{
    // Default-initialise arrays to clear state: no faults, all DTC slots kNoFault.
    faulted_.fill(false);
    active_dtcs_.fill(domain::FaultCode::kNoFault);
}

FaultManagerStatus FaultManager::InjectFault(
    const domain::LampFunction function) noexcept
{
    const std::size_t index = LampFunctionToIndex(function);

    if (index >= faulted_.size())
    {
        return FaultManagerStatus::kInvalidFunction;
    }

    if (faulted_[index])
    {
        return FaultManagerStatus::kAlreadyFaulted;
    }

    faulted_[index] = true;

    // Append the DTC to the active list if there is capacity.
    if (active_fault_count_ < static_cast<std::uint8_t>(active_dtcs_.size()))
    {
        active_dtcs_[active_fault_count_] = domain::LampFunctionToFaultCode(function);
        ++active_fault_count_;
    }

    return FaultManagerStatus::kSuccess;
}

FaultManagerStatus FaultManager::ClearFault(
    const domain::LampFunction function) noexcept
{
    const std::size_t index = LampFunctionToIndex(function);

    if (index >= faulted_.size())
    {
        return FaultManagerStatus::kInvalidFunction;
    }

    if (!faulted_[index])
    {
        return FaultManagerStatus::kNotFaulted;
    }

    faulted_[index] = false;

    // Remove the DTC from the compacted array by shifting remaining entries left.
    const domain::FaultCode target_code = domain::LampFunctionToFaultCode(function);

    for (std::uint8_t i = 0U; i < active_fault_count_; ++i)
    {
        if (active_dtcs_[i] == target_code)
        {
            // Shift everything after slot i one position to the left.
            for (std::uint8_t j = i; j + 1U < active_fault_count_; ++j)
            {
                active_dtcs_[j] = active_dtcs_[j + 1U];
            }
            active_dtcs_[active_fault_count_ - 1U] = domain::FaultCode::kNoFault;
            --active_fault_count_;
            break;
        }
    }

    return FaultManagerStatus::kSuccess;
}

void FaultManager::ClearAllFaults() noexcept
{
    faulted_.fill(false);
    active_dtcs_.fill(domain::FaultCode::kNoFault);
    active_fault_count_ = 0U;
}

bool FaultManager::IsFaulted(
    const domain::LampFunction function) const noexcept
{
    const std::size_t index = LampFunctionToIndex(function);

    if (index >= faulted_.size())
    {
        return false;
    }

    return faulted_[index];
}

std::uint8_t FaultManager::ActiveFaultCount() const noexcept
{
    return active_fault_count_;
}

domain::LampFaultStatus FaultManager::GetFaultStatus() const noexcept
{
    domain::LampFaultStatus status {};
    status.fault_present     = (active_fault_count_ > 0U);
    status.active_fault_count = active_fault_count_;
    status.active_faults      = active_dtcs_;
    return status;
}

void FaultManager::PopulateHealth(domain::NodeHealthStatus& health) const noexcept
{
    health.lamp_driver_fault_present = (active_fault_count_ > 0U);
    health.active_fault_count        = active_fault_count_;

    // Update health_state: if node was Operational and a fault appeared, degrade.
    if (health.lamp_driver_fault_present &&
        health.health_state == domain::NodeHealthState::kOperational)
    {
        health.health_state = domain::NodeHealthState::kFaulted;
    }
    else if (
        !health.lamp_driver_fault_present &&
        health.health_state == domain::NodeHealthState::kFaulted)
    {
        // Fault cleared: step back to Operational if link and service are up.
        if (health.ethernet_link_available && health.service_available)
        {
            health.health_state = domain::NodeHealthState::kOperational;
        }
    }
}

std::size_t FaultManager::LampFunctionToIndex(
    const domain::LampFunction function) noexcept
{
    std::size_t index {static_cast<std::size_t>(-1)};

    switch (function)
    {
    case domain::LampFunction::kLeftIndicator:
        index = 0U;
        break;
    case domain::LampFunction::kRightIndicator:
        index = 1U;
        break;
    case domain::LampFunction::kHazardLamp:
        index = 2U;
        break;
    case domain::LampFunction::kParkLamp:
        index = 3U;
        break;
    case domain::LampFunction::kHeadLamp:
        index = 4U;
        break;
    case domain::LampFunction::kUnknown:
    default:
        break;
    }

    return index;
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control

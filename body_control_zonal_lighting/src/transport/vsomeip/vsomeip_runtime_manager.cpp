#include "body_control/lighting/transport/vsomeip/vsomeip_runtime_manager.hpp"

namespace body_control::lighting::transport::vsomeip
{

bool VsomeipRuntimeManager::Initialize()
{
    is_initialized_ = true;
    return true;
}

void VsomeipRuntimeManager::Shutdown() noexcept
{
    is_initialized_ = false;
    available_service_id_ = 0U;
    available_instance_id_ = 0U;
    service_available_ = false;
}

bool VsomeipRuntimeManager::IsInitialized() const noexcept
{
    return is_initialized_;
}

void VsomeipRuntimeManager::SetServiceAvailability(
    const std::uint16_t service_id,
    const std::uint16_t instance_id,
    const bool is_available) noexcept
{
    available_service_id_ = service_id;
    available_instance_id_ = instance_id;
    service_available_ = is_available;
}

bool VsomeipRuntimeManager::IsServiceAvailable(
    const std::uint16_t service_id,
    const std::uint16_t instance_id) const noexcept
{
    return is_initialized_ &&
           service_available_ &&
           (available_service_id_ == service_id) &&
           (available_instance_id_ == instance_id);
}

}  // namespace body_control::lighting::transport::vsomeip
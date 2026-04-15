#include "body_control/lighting/service/rear_lighting_service_provider.hpp"

namespace body_control::lighting::service
{

RearLightingServiceProvider::RearLightingServiceProvider(
    application::RearLightingFunctionManager& rear_lighting_function_manager,
    transport::TransportAdapterInterface& transport_adapter) noexcept
    : rear_lighting_function_manager_(rear_lighting_function_manager)
    , transport_adapter_(transport_adapter)
    , is_initialized_(false)
{
}

ServiceStatus RearLightingServiceProvider::Initialize()
{
    is_initialized_ = true;
    return ServiceStatus::kSuccess;
}

ServiceStatus RearLightingServiceProvider::Shutdown()
{
    is_initialized_ = false;
    return ServiceStatus::kSuccess;
}

}  // namespace body_control::lighting::service
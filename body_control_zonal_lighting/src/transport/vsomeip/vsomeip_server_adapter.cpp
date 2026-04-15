#include <memory>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace vsomeip
{

std::unique_ptr<TransportAdapterInterface>
CreateRearLightingNodeRuntimeAdapter();

std::unique_ptr<TransportAdapterInterface>
CreateRearLightingNodeVsomeipServerAdapter()
{
    return CreateRearLightingNodeRuntimeAdapter();
}

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control
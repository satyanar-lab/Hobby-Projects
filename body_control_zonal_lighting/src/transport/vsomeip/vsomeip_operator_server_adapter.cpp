// Named shim that exposes a stable public symbol for the CZC operator-service
// vsomeip server.  Implementation lives in vsomeip_runtime_manager.cpp.

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
CreateControllerOperatorRuntimeAdapter();

std::unique_ptr<TransportAdapterInterface>
CreateControllerOperatorVsomeipServerAdapter()
{
    return CreateControllerOperatorRuntimeAdapter();
}

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control

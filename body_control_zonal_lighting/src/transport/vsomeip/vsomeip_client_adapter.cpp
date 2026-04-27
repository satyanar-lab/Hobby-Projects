// Named shim that exposes a stable public symbol for the CZC vsomeip client.
// The implementation lives in vsomeip_runtime_manager.cpp; this thin wrapper
// keeps the public API surface independent of that combined translation unit.

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
CreateCentralZoneControllerRuntimeAdapter();

std::unique_ptr<TransportAdapterInterface>
CreateCentralZoneControllerVsomeipClientAdapter()
{
    return CreateCentralZoneControllerRuntimeAdapter();
}

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control
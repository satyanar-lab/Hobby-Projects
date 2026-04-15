#include <memory>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace ethernet
{

std::unique_ptr<TransportAdapterInterface>
CreateCentralZoneControllerUdpTransportAdapter();

std::unique_ptr<TransportAdapterInterface>
CreateRearLightingNodeUdpTransportAdapter();

}  // namespace ethernet

namespace vsomeip
{

std::unique_ptr<TransportAdapterInterface>
CreateCentralZoneControllerRuntimeAdapter()
{
    return ethernet::CreateCentralZoneControllerUdpTransportAdapter();
}

std::unique_ptr<TransportAdapterInterface>
CreateRearLightingNodeRuntimeAdapter()
{
    return ethernet::CreateRearLightingNodeUdpTransportAdapter();
}

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control
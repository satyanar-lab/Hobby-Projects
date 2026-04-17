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

std::unique_ptr<TransportAdapterInterface>
CreateControllerOperatorUdpTransportAdapter();

std::unique_ptr<TransportAdapterInterface>
CreateOperatorClientUdpTransportAdapter();

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

std::unique_ptr<TransportAdapterInterface>
CreateControllerOperatorRuntimeAdapter()
{
    return ethernet::CreateControllerOperatorUdpTransportAdapter();
}

std::unique_ptr<TransportAdapterInterface>
CreateOperatorClientRuntimeAdapter()
{
    return ethernet::CreateOperatorClientUdpTransportAdapter();
}

}  // namespace vsomeip
}  // namespace transport
}  // namespace lighting
}  // namespace body_control
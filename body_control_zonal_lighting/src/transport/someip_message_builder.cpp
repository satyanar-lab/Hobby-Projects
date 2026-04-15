#include "body_control/lighting/transport/someip_message_builder.hpp"

#include "body_control/lighting/domain/lighting_service_ids.hpp"

namespace body_control::lighting::transport
{
namespace
{

[[nodiscard]] TransportMessage BuildMessage(
    const std::uint16_t message_id,
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    TransportMessage message {};
    message.service_id = domain::service_ids::kRearLightingServiceId;
    message.instance_id = domain::service_ids::kRearLightingServiceInstanceId;
    message.message_id = message_id;
    message.payload_data = payload_data;
    message.payload_length = payload_length;

    return message;
}

}  // namespace

TransportMessage SomeIpMessageBuilder::BuildSetLampCommandRequest(
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    return BuildMessage(
        domain::service_ids::kSetLampCommandMethodId,
        payload_data,
        payload_length);
}

TransportMessage SomeIpMessageBuilder::BuildGetLampStatusRequest(
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    return BuildMessage(
        domain::service_ids::kGetLampStatusMethodId,
        payload_data,
        payload_length);
}

TransportMessage SomeIpMessageBuilder::BuildGetNodeHealthRequest(
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    return BuildMessage(
        domain::service_ids::kGetNodeHealthMethodId,
        payload_data,
        payload_length);
}

TransportMessage SomeIpMessageBuilder::BuildLampStatusResponse(
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    return BuildMessage(
        domain::service_ids::kGetLampStatusMethodId,
        payload_data,
        payload_length);
}

TransportMessage SomeIpMessageBuilder::BuildNodeHealthResponse(
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    return BuildMessage(
        domain::service_ids::kGetNodeHealthMethodId,
        payload_data,
        payload_length);
}

TransportMessage SomeIpMessageBuilder::BuildLampStatusEvent(
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    return BuildMessage(
        domain::service_ids::kLampStatusEventId,
        payload_data,
        payload_length);
}

TransportMessage SomeIpMessageBuilder::BuildNodeHealthEvent(
    const std::uint8_t* payload_data,
    const std::size_t payload_length) noexcept
{
    return BuildMessage(
        domain::service_ids::kNodeHealthEventId,
        payload_data,
        payload_length);
}

}  // namespace body_control::lighting::transport
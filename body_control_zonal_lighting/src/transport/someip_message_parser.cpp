#include "body_control/lighting/transport/someip_message_parser.hpp"

#include "body_control/lighting/domain/lighting_payload_codec.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"

namespace body_control::lighting::transport
{
namespace
{

[[nodiscard]] bool IsRearLightingMessage(
    const TransportMessage& message) noexcept
{
    return (message.service_id == domain::service_ids::kRearLightingServiceId) &&
           (message.instance_id == domain::service_ids::kRearLightingServiceInstanceId);
}

}  // namespace

ParsedSomeIpMessage SomeIpMessageParser::Parse(
    const TransportMessage& message) noexcept
{
    ParsedSomeIpMessage parsed_message {};
    parsed_message.message_type = SomeIpMessageType::kUnknown;
    parsed_message.is_valid = false;

    if (!IsRearLightingMessage(message))
    {
        return parsed_message;
    }

    switch (message.message_id)
    {
    case domain::service_ids::kSetLampCommandMethodId:
        if (message.payload_length == domain::kLampCommandPayloadLength)
        {
            parsed_message.message_type = SomeIpMessageType::kSetLampCommandRequest;
            parsed_message.is_valid = true;
        }
        break;

    case domain::service_ids::kGetLampStatusMethodId:
        if (message.payload_length == 1U)
        {
            parsed_message.message_type = SomeIpMessageType::kGetLampStatusRequest;
            parsed_message.is_valid = true;
        }
        else if (message.payload_length == domain::kLampStatusPayloadLength)
        {
            parsed_message.message_type = SomeIpMessageType::kLampStatusResponse;
            parsed_message.is_valid = true;
        }
        break;

    case domain::service_ids::kGetNodeHealthMethodId:
        if (message.payload_length == 0U)
        {
            parsed_message.message_type = SomeIpMessageType::kGetNodeHealthRequest;
            parsed_message.is_valid = true;
        }
        else if (message.payload_length == domain::kNodeHealthStatusPayloadLength)
        {
            parsed_message.message_type = SomeIpMessageType::kNodeHealthResponse;
            parsed_message.is_valid = true;
        }
        break;

    case domain::service_ids::kLampStatusEventId:
        if (message.payload_length == domain::kLampStatusPayloadLength)
        {
            parsed_message.message_type = SomeIpMessageType::kLampStatusEvent;
            parsed_message.is_valid = true;
        }
        break;

    case domain::service_ids::kNodeHealthEventId:
        if (message.payload_length == domain::kNodeHealthStatusPayloadLength)
        {
            parsed_message.message_type = SomeIpMessageType::kNodeHealthEvent;
            parsed_message.is_valid = true;
        }
        break;

    default:
        break;
    }

    return parsed_message;
}

}  // namespace body_control::lighting::transport
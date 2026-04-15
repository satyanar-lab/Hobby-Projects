#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP_
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control::lighting::transport
{

/**
 * @brief Classified SOME/IP-style message type used in this project.
 */
enum class SomeIpMessageType : std::uint8_t
{
    kUnknown = 0U,
    kSetLampCommandRequest = 1U,
    kGetLampStatusRequest = 2U,
    kGetNodeHealthRequest = 3U,
    kLampStatusResponse = 4U,
    kNodeHealthResponse = 5U,
    kLampStatusEvent = 6U,
    kNodeHealthEvent = 7U
};

/**
 * @brief Parsed result for a transport message.
 */
struct ParsedSomeIpMessage
{
    SomeIpMessageType message_type {SomeIpMessageType::kUnknown};
    bool is_rear_lighting_service_message {false};
    bool is_valid {false};
};

/**
 * @brief Utility class for classifying incoming SOME/IP-style messages.
 */
class SomeIpMessageParser
{
public:
    SomeIpMessageParser() = delete;
    ~SomeIpMessageParser() = delete;

    [[nodiscard]] static ParsedSomeIpMessage Parse(
        const TransportMessage& transport_message) noexcept;

    [[nodiscard]] static bool IsRearLightingServiceMessage(
        const TransportMessage& transport_message) noexcept;
};

}  // namespace body_control::lighting::transport

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_PARSER_HPP_
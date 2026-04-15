#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP_
#define BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP_

#include <cstddef>
#include <cstdint>

#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control::lighting::transport
{

/**
 * @brief Utility class for constructing transport messages using the project
 *        SOME/IP-style service identifiers.
 */
class SomeIpMessageBuilder
{
public:
    SomeIpMessageBuilder() = delete;
    ~SomeIpMessageBuilder() = delete;

    [[nodiscard]] static TransportMessage BuildSetLampCommandRequest(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    [[nodiscard]] static TransportMessage BuildGetLampStatusRequest(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    [[nodiscard]] static TransportMessage BuildGetNodeHealthRequest(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    [[nodiscard]] static TransportMessage BuildLampStatusResponse(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    [[nodiscard]] static TransportMessage BuildNodeHealthResponse(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    [[nodiscard]] static TransportMessage BuildLampStatusEvent(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;

    [[nodiscard]] static TransportMessage BuildNodeHealthEvent(
        const std::uint8_t* payload_data,
        std::size_t payload_length) noexcept;
};

}  // namespace body_control::lighting::transport

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_SOMEIP_MESSAGE_BUILDER_HPP_
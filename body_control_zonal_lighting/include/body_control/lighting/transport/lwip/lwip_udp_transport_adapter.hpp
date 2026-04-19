#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_LWIP_UDP_TRANSPORT_ADAPTER_HPP_
#define BODY_CONTROL_LIGHTING_TRANSPORT_LWIP_UDP_TRANSPORT_ADAPTER_HPP_

#include <cstdint>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

struct udp_pcb;
struct pbuf;
struct ip_addr;

namespace body_control::lighting::transport::lwip
{

struct LwipUdpConfig
{
    std::uint32_t remote_ip_addr;   // host-byte-order IPv4, e.g. PP_HTONL(0xC0A8000A)
    std::uint16_t local_port;
    std::uint16_t remote_port;
};

class LwipUdpTransportAdapter final : public TransportAdapterInterface
{
public:
    explicit LwipUdpTransportAdapter(const LwipUdpConfig& config) noexcept;
    ~LwipUdpTransportAdapter() override;

    LwipUdpTransportAdapter(const LwipUdpTransportAdapter&) = delete;
    LwipUdpTransportAdapter& operator=(const LwipUdpTransportAdapter&) = delete;
    LwipUdpTransportAdapter(LwipUdpTransportAdapter&&) = delete;
    LwipUdpTransportAdapter& operator=(LwipUdpTransportAdapter&&) = delete;

    [[nodiscard]] TransportStatus Initialize() override;
    [[nodiscard]] TransportStatus Shutdown() override;

    [[nodiscard]] TransportStatus SendRequest(
        const TransportMessage& transport_message) override;

    [[nodiscard]] TransportStatus SendResponse(
        const TransportMessage& transport_message) override;

    [[nodiscard]] TransportStatus SendEvent(
        const TransportMessage& transport_message) override;

    void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept override;

private:
    [[nodiscard]] TransportStatus SendMessage(
        const TransportMessage& transport_message,
        std::uint16_t message_kind);

    static void RecvCallback(
        void* arg,
        udp_pcb* pcb,
        pbuf* p,
        const ip_addr* addr,
        std::uint16_t port);

    LwipUdpConfig config_;
    udp_pcb* pcb_ {nullptr};
    bool is_initialized_ {false};
    TransportMessageHandlerInterface* handler_ {nullptr};
};

}  // namespace body_control::lighting::transport::lwip

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_LWIP_UDP_TRANSPORT_ADAPTER_HPP_

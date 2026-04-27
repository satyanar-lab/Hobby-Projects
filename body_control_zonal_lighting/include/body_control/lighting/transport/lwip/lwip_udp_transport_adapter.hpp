#ifndef BODY_CONTROL_LIGHTING_TRANSPORT_LWIP_UDP_TRANSPORT_ADAPTER_HPP_
#define BODY_CONTROL_LIGHTING_TRANSPORT_LWIP_UDP_TRANSPORT_ADAPTER_HPP_

#include <cstdint>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

// LwIP forward declarations — udp_pcb and pbuf are real structs; ip_addr_t is
// a typedef so it cannot be forward-declared; include ip_addr.h for it.
#include "lwip/ip_addr.h"

struct udp_pcb;
struct pbuf;

namespace body_control::lighting::transport::lwip
{

/**
 * Configuration for a single LwIP UDP endpoint.
 *
 * The adapter opens one UDP socket that binds to local_port and sends to
 * remote_ip_addr:remote_port.  All three fields must be set before calling
 * LwipUdpTransportAdapter::Initialize().
 */
struct LwipUdpConfig
{
    /// Destination IPv4 address in host byte order (e.g. PP_HTONL(0xC0A8000A) for 192.168.0.10).
    std::uint32_t remote_ip_addr;

    /// UDP port on which this adapter listens for inbound datagrams.
    std::uint16_t local_port;

    /// UDP port of the remote peer to which outbound datagrams are sent.
    std::uint16_t remote_port;
};

/**
 * STM32 transport adapter backed by LwIP raw UDP.
 *
 * Used on the NUCLEO-H753ZI board where LwIP provides the TCP/IP stack
 * over the on-chip Ethernet MAC.  Serialises TransportMessage structs into
 * a minimal SOME/IP-style 12-byte header + payload and fires them as UDP
 * datagrams; inbound datagrams are parsed and forwarded to the registered
 * TransportMessageHandlerInterface.
 *
 * All three send methods (SendRequest, SendResponse, SendEvent) delegate to
 * the private SendMessage() path — the message_kind byte in the header is
 * the only difference between them, which is meaningful for future reliable-
 * transport extensions but not acted on by the receiver today.
 *
 * RecvCallback() is a static C-linkage function registered with the LwIP
 * udp_recv() API; it casts the `arg` void pointer back to the adapter
 * instance and dispatches into the handler.
 */
class LwipUdpTransportAdapter final : public TransportAdapterInterface
{
public:
    /**
     * Stores the configuration.  Does not open a socket; call Initialize() first.
     *
     * @param config  IP address, local port, and remote port for this endpoint.
     */
    explicit LwipUdpTransportAdapter(const LwipUdpConfig& config) noexcept;

    /** Closes the UDP PCB if open. */
    ~LwipUdpTransportAdapter() override;

    LwipUdpTransportAdapter(const LwipUdpTransportAdapter&) = delete;
    LwipUdpTransportAdapter& operator=(const LwipUdpTransportAdapter&) = delete;
    LwipUdpTransportAdapter(LwipUdpTransportAdapter&&) = delete;
    LwipUdpTransportAdapter& operator=(LwipUdpTransportAdapter&&) = delete;

    /**
     * Allocates a LwIP UDP PCB, binds to the local port, and registers
     * RecvCallback() as the receive handler.
     *
     * @return kSuccess, or kTransmissionFailed if udp_new() or udp_bind() fails.
     */
    [[nodiscard]] TransportStatus Initialize() override;

    /**
     * Removes the UDP PCB and sets is_initialized_ to false.
     *
     * @return kSuccess; never fails.
     */
    [[nodiscard]] TransportStatus Shutdown() override;

    /** Encodes and sends transport_message as a request datagram. */
    [[nodiscard]] TransportStatus SendRequest(
        const TransportMessage& transport_message) override;

    /** Encodes and sends transport_message as a response datagram. */
    [[nodiscard]] TransportStatus SendResponse(
        const TransportMessage& transport_message) override;

    /** Encodes and sends transport_message as an event datagram. */
    [[nodiscard]] TransportStatus SendEvent(
        const TransportMessage& transport_message) override;

    /** Registers the handler that receives decoded inbound messages. */
    void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept override;

private:
    /**
     * Common send path: serialises the message header and payload into a LwIP
     * pbuf and calls udp_sendto().
     *
     * @param message_kind  Single-byte type indicator written into the header
     *                      (0x01 = request, 0x02 = response, 0x03 = event).
     */
    [[nodiscard]] TransportStatus SendMessage(
        const TransportMessage& transport_message,
        std::uint16_t message_kind);

    /**
     * LwIP receive callback, called from the LwIP task when a UDP datagram
     * arrives on the bound port.
     *
     * Parses the SOME/IP header, constructs a TransportMessage, and calls
     * handler_->OnTransportMessageReceived().  The pbuf is freed after the call.
     *
     * @param arg   Pointer to the owning LwipUdpTransportAdapter instance.
     * @param pcb   The receiving UDP PCB (not used after the callback).
     * @param p     Received pbuf chain; must be freed before returning.
     * @param addr  Source IP address of the datagram.
     * @param port  Source UDP port of the datagram.
     */
    static void RecvCallback(
        void* arg,
        udp_pcb* pcb,
        pbuf* p,
        const ip_addr_t* addr,
        std::uint16_t port);

    LwipUdpConfig config_;

    /// LwIP protocol control block; nullptr until Initialize() succeeds.
    udp_pcb* pcb_ {nullptr};

    bool is_initialized_ {false};

    /// Non-owning pointer to the handler that processes decoded messages.
    TransportMessageHandlerInterface* handler_ {nullptr};
};

}  // namespace body_control::lighting::transport::lwip

#endif  // BODY_CONTROL_LIGHTING_TRANSPORT_LWIP_UDP_TRANSPORT_ADAPTER_HPP_

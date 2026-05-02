#ifndef BODY_CONTROL_LIGHTING_PLATFORM_ZEPHYR_UDP_TRANSPORT_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_ZEPHYR_UDP_TRANSPORT_HPP_

#include <cstdint>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

namespace body_control::lighting::transport::zephyr_transport
{

/** Configuration for the Zephyr BSD-socket UDP endpoint. */
struct ZephyrUdpConfig
{
    /// Remote IPv4 address string (e.g. "192.168.0.10" for the Linux CZC).
    const char*    remote_ip_addr;

    /// UDP port on which this node listens for inbound datagrams.
    std::uint16_t  local_port;

    /// UDP port of the remote peer to which outbound datagrams are sent.
    std::uint16_t  remote_port;
};

/** Zephyr RTOS transport adapter backed by a BSD-socket UDP socket.
 *
 *  Implements the same frame encoding as LwipUdpTransportAdapter so Linux
 *  and Zephyr endpoints are wire-compatible.
 *
 *  Initialize() opens an AF_INET / SOCK_DGRAM socket and binds to local_port.
 *  SendEvent/Request/Response() serialise the 20-byte frame header + payload
 *  and call zsock_sendto().
 *
 *  ReceiveBlocking() blocks on zsock_recvfrom() until one datagram arrives,
 *  decodes the frame header, and calls handler_->OnTransportMessageReceived().
 *  The UDP receive thread in main.cpp calls ReceiveBlocking() in a tight loop.
 *  ReceiveBlocking is NOT part of TransportAdapterInterface — it is a
 *  Zephyr-specific entry point for the dedicated receive thread. */
class ZephyrUdpTransportAdapter final : public TransportAdapterInterface
{
public:
    explicit ZephyrUdpTransportAdapter(const ZephyrUdpConfig& config) noexcept;
    ~ZephyrUdpTransportAdapter() override;

    ZephyrUdpTransportAdapter(const ZephyrUdpTransportAdapter&)            = delete;
    ZephyrUdpTransportAdapter& operator=(const ZephyrUdpTransportAdapter&) = delete;
    ZephyrUdpTransportAdapter(ZephyrUdpTransportAdapter&&)                 = delete;
    ZephyrUdpTransportAdapter& operator=(ZephyrUdpTransportAdapter&&)      = delete;

    /** Opens a UDP socket and binds to local_port. */
    [[nodiscard]] TransportStatus Initialize() override;

    /** Closes the socket. */
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

    /** Blocks until one UDP datagram arrives, decodes it, and dispatches to
     *  the registered handler.  Returns immediately if not initialized.
     *  Called repeatedly by the UDP receive thread. */
    void ReceiveBlocking() noexcept;

private:
    /** Common send path: encodes the frame and calls zsock_sendto(). */
    [[nodiscard]] TransportStatus SendMessage(
        const TransportMessage& transport_message,
        std::uint8_t            message_kind) noexcept;

    ZephyrUdpConfig config_;

    /// Socket file descriptor; -1 until Initialize() succeeds.
    int  sock_          {-1};
    bool is_initialized_{false};

    TransportMessageHandlerInterface* handler_ {nullptr};
};

}  // namespace body_control::lighting::transport::zephyr_transport

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_ZEPHYR_UDP_TRANSPORT_HPP_

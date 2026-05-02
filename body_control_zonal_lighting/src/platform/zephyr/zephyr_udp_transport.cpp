#include "zephyr_udp_transport.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/sys/byteorder.h>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"

LOG_MODULE_REGISTER(udp_transport, LOG_LEVEL_DBG);

namespace body_control::lighting::transport::zephyr_transport
{

// ---- Frame encode / decode -------------------------------------------------
// Wire format is byte-for-byte identical to lwip_udp_transport_adapter.cpp
// so the Linux CZC and the Zephyr node interoperate without conversion.
// byte-swap uses Zephyr sys_cpu_to_be16/32 instead of POSIX htons/htonl.

namespace
{

constexpr std::uint16_t kFrameVersion       {1U};
constexpr std::uint16_t kFlagIsEventMask    {0x0001U};
constexpr std::uint16_t kFlagIsReliableMask {0x0002U};

constexpr std::uint8_t kRequestMessageKind  {1U};
constexpr std::uint8_t kResponseMessageKind {2U};
constexpr std::uint8_t kEventMessageKind    {3U};

struct FrameHeader
{
    std::uint16_t version;
    std::uint16_t message_kind;
    std::uint16_t service_id;
    std::uint16_t instance_id;
    std::uint16_t method_or_event_id;
    std::uint16_t client_id;
    std::uint16_t session_id;
    std::uint16_t flags;
    std::uint32_t payload_length;
};

constexpr std::size_t kFrameHeaderSize {sizeof(FrameHeader)};  // 20 bytes

std::vector<std::uint8_t> EncodeFrame(
    const TransportMessage& msg,
    const std::uint8_t      message_kind)
{
    const std::size_t payload_len = msg.payload.size();
    std::vector<std::uint8_t> buf(kFrameHeaderSize + payload_len, 0U);

    std::uint16_t flags {0U};
    if (msg.is_event)    { flags = static_cast<std::uint16_t>(flags | kFlagIsEventMask);    }
    if (msg.is_reliable) { flags = static_cast<std::uint16_t>(flags | kFlagIsReliableMask); }

    FrameHeader hdr {};
    hdr.version            = sys_cpu_to_be16(kFrameVersion);
    hdr.message_kind       = sys_cpu_to_be16(static_cast<std::uint16_t>(message_kind));
    hdr.service_id         = sys_cpu_to_be16(msg.service_id);
    hdr.instance_id        = sys_cpu_to_be16(msg.instance_id);
    hdr.method_or_event_id = sys_cpu_to_be16(msg.method_or_event_id);
    hdr.client_id          = sys_cpu_to_be16(msg.client_id);
    hdr.session_id         = sys_cpu_to_be16(msg.session_id);
    hdr.flags              = sys_cpu_to_be16(flags);
    hdr.payload_length     = sys_cpu_to_be32(static_cast<std::uint32_t>(payload_len));

    std::memcpy(buf.data(), &hdr, kFrameHeaderSize);
    if (payload_len > 0U)
    {
        std::memcpy(buf.data() + kFrameHeaderSize,
                    msg.payload.data(), payload_len);
    }
    return buf;
}

bool DecodeFrame(
    const std::uint8_t* const data,
    const std::size_t         length,
    TransportMessage&         msg_out) noexcept
{
    if ((data == nullptr) || (length < kFrameHeaderSize)) { return false; }

    FrameHeader hdr {};
    std::memcpy(&hdr, data, kFrameHeaderSize);

    const std::uint16_t version = sys_be16_to_cpu(hdr.version);
    if (version != kFrameVersion) { return false; }

    const std::uint32_t payload_len =
        sys_be32_to_cpu(hdr.payload_length);
    if (length != kFrameHeaderSize + static_cast<std::size_t>(payload_len))
    {
        return false;
    }

    const std::uint16_t flags = sys_be16_to_cpu(hdr.flags);

    msg_out.service_id         = sys_be16_to_cpu(hdr.service_id);
    msg_out.instance_id        = sys_be16_to_cpu(hdr.instance_id);
    msg_out.method_or_event_id = sys_be16_to_cpu(hdr.method_or_event_id);
    msg_out.client_id          = sys_be16_to_cpu(hdr.client_id);
    msg_out.session_id         = sys_be16_to_cpu(hdr.session_id);
    msg_out.is_event    = ((flags & kFlagIsEventMask)    != 0U);
    msg_out.is_reliable = ((flags & kFlagIsReliableMask) != 0U);
    msg_out.payload.clear();

    if (payload_len > 0U)
    {
        msg_out.payload.resize(static_cast<std::size_t>(payload_len));
        std::memcpy(msg_out.payload.data(),
                    data + kFrameHeaderSize,
                    static_cast<std::size_t>(payload_len));
    }
    return true;
}

}  // namespace

// ---- ZephyrUdpTransportAdapter ---------------------------------------------

ZephyrUdpTransportAdapter::ZephyrUdpTransportAdapter(
    const ZephyrUdpConfig& config) noexcept
    : config_(config)
{
}

ZephyrUdpTransportAdapter::~ZephyrUdpTransportAdapter()
{
    static_cast<void>(Shutdown());
}

TransportStatus ZephyrUdpTransportAdapter::Initialize()
{
    if (is_initialized_) { return TransportStatus::kSuccess; }

    sock_ = zsock_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock_ < 0) { return TransportStatus::kTransmissionFailed; }

    struct sockaddr_in local_addr {};
    local_addr.sin_family      = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port        = htons(config_.local_port);

    if (zsock_bind(sock_,
                   reinterpret_cast<const struct sockaddr*>(&local_addr),
                   sizeof(local_addr)) < 0)
    {
        zsock_close(sock_);
        sock_ = -1;
        return TransportStatus::kTransmissionFailed;
    }

    is_initialized_ = true;

    if (handler_ != nullptr)
    {
        handler_->OnTransportAvailabilityChanged(true);
    }

    return TransportStatus::kSuccess;
}

TransportStatus ZephyrUdpTransportAdapter::Shutdown()
{
    if (!is_initialized_) { return TransportStatus::kSuccess; }

    zsock_close(sock_);
    sock_ = -1;
    is_initialized_ = false;

    if (handler_ != nullptr)
    {
        handler_->OnTransportAvailabilityChanged(false);
    }

    return TransportStatus::kSuccess;
}

TransportStatus ZephyrUdpTransportAdapter::SendRequest(
    const TransportMessage& transport_message)
{
    return SendMessage(transport_message, kRequestMessageKind);
}

TransportStatus ZephyrUdpTransportAdapter::SendResponse(
    const TransportMessage& transport_message)
{
    return SendMessage(transport_message, kResponseMessageKind);
}

TransportStatus ZephyrUdpTransportAdapter::SendEvent(
    const TransportMessage& transport_message)
{
    return SendMessage(transport_message, kEventMessageKind);
}

void ZephyrUdpTransportAdapter::SetMessageHandler(
    TransportMessageHandlerInterface* message_handler) noexcept
{
    handler_ = message_handler;
}

TransportStatus ZephyrUdpTransportAdapter::SendMessage(
    const TransportMessage& transport_message,
    const std::uint8_t      message_kind) noexcept
{
    if (!is_initialized_) { return TransportStatus::kNotInitialized; }

    const std::vector<std::uint8_t> frame =
        EncodeFrame(transport_message, message_kind);

    struct sockaddr_in remote_addr {};
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port   = htons(config_.remote_port);
    // net_addr_pton converts the IP string to binary in-place.
    if (net_addr_pton(AF_INET, config_.remote_ip_addr,
                      &remote_addr.sin_addr) < 0)
    {
        return TransportStatus::kInvalidArgument;
    }

    const ssize_t sent = zsock_sendto(
        sock_,
        frame.data(),
        frame.size(),
        0,
        reinterpret_cast<const struct sockaddr*>(&remote_addr),
        sizeof(remote_addr));

    return (sent == static_cast<ssize_t>(frame.size()))
               ? TransportStatus::kSuccess
               : TransportStatus::kTransmissionFailed;
}

void ZephyrUdpTransportAdapter::ReceiveBlocking() noexcept
{
    if (!is_initialized_ || (handler_ == nullptr)) { return; }

    // 256 bytes covers the 20-byte frame header plus any lamp-command payload.
    std::uint8_t buf[256];

    struct sockaddr_in src_addr {};
    socklen_t          src_len = sizeof(src_addr);

    const ssize_t received = zsock_recvfrom(
        sock_,
        buf, sizeof(buf),
        0,
        reinterpret_cast<struct sockaddr*>(&src_addr),
        &src_len);

    if (received < 0)
    {
        LOG_WRN("UDP RX: recv error errno=%d", errno);
        // Yield for 10 ms to prevent tight spin if the error persists
        // (e.g., ENETDOWN while the Ethernet interface initialises).
        k_sleep(K_MSEC(10));
        return;
    }
    if (received == 0) { return; }

    LOG_INF("UDP RX: %d bytes from %d.%d.%d.%d",
            static_cast<int>(received),
            (src_addr.sin_addr.s_addr      ) & 0xFFU,
            (src_addr.sin_addr.s_addr >>  8) & 0xFFU,
            (src_addr.sin_addr.s_addr >> 16) & 0xFFU,
            (src_addr.sin_addr.s_addr >> 24) & 0xFFU);

    TransportMessage msg {};
    if (!DecodeFrame(buf, static_cast<std::size_t>(received), msg))
    {
        LOG_WRN("UDP RX: decode failed (len=%d, need >=20)", static_cast<int>(received));
        return;
    }

    LOG_DBG("UDP RX: svc=0x%04x evt=0x%04x payload=%u",
            msg.service_id, msg.method_or_event_id,
            static_cast<unsigned>(msg.payload.size()));

    handler_->OnTransportMessageReceived(msg);
}

}  // namespace body_control::lighting::transport::zephyr_transport

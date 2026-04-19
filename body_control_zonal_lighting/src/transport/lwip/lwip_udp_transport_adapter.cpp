#include "body_control/lighting/transport/lwip/lwip_udp_transport_adapter.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

#include "lwip/def.h"
#include "lwip/ip_addr.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"

#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control::lighting::transport::lwip
{

// ---- Frame encode / decode -------------------------------------------------
// Self-contained copy of the wire format from ethernet_frame_adapter.cpp,
// using lwip_htons/lwip_htonl instead of <arpa/inet.h> so this TU is
// portable to bare-metal targets that have no POSIX headers.

namespace
{

constexpr std::uint16_t kFrameVersion      {1U};
constexpr std::uint16_t kFlagIsEventMask   {0x0001U};
constexpr std::uint16_t kFlagIsReliableMask{0x0002U};

constexpr std::uint16_t kRequestMessageKind {1U};
constexpr std::uint16_t kResponseMessageKind{2U};
constexpr std::uint16_t kEventMessageKind   {3U};

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

constexpr std::size_t kFrameHeaderSize = sizeof(FrameHeader);

std::vector<std::uint8_t> EncodeFrame(
    const TransportMessage& msg,
    const std::uint16_t message_kind)
{
    const std::size_t payload_len = msg.payload.size();
    std::vector<std::uint8_t> buf(kFrameHeaderSize + payload_len, 0U);

    std::uint16_t flags {0U};
    if (msg.is_event)    { flags = static_cast<std::uint16_t>(flags | kFlagIsEventMask);    }
    if (msg.is_reliable) { flags = static_cast<std::uint16_t>(flags | kFlagIsReliableMask); }

    FrameHeader hdr {};
    hdr.version           = lwip_htons(kFrameVersion);
    hdr.message_kind      = lwip_htons(message_kind);
    hdr.service_id        = lwip_htons(msg.service_id);
    hdr.instance_id       = lwip_htons(msg.instance_id);
    hdr.method_or_event_id= lwip_htons(msg.method_or_event_id);
    hdr.client_id         = lwip_htons(msg.client_id);
    hdr.session_id        = lwip_htons(msg.session_id);
    hdr.flags             = lwip_htons(flags);
    hdr.payload_length    = lwip_htonl(static_cast<std::uint32_t>(payload_len));

    std::memcpy(buf.data(), &hdr, kFrameHeaderSize);
    if (payload_len > 0U)
    {
        std::memcpy(buf.data() + kFrameHeaderSize,
                    msg.payload.data(),
                    payload_len);
    }
    return buf;
}

bool DecodeFrame(
    const std::uint8_t* const data,
    const std::size_t          length,
    TransportMessage&          msg_out,
    std::uint16_t&             kind_out) noexcept
{
    if ((data == nullptr) || (length < kFrameHeaderSize)) { return false; }

    FrameHeader hdr {};
    std::memcpy(&hdr, data, kFrameHeaderSize);

    hdr.version            = lwip_ntohs(hdr.version);
    hdr.message_kind       = lwip_ntohs(hdr.message_kind);
    hdr.service_id         = lwip_ntohs(hdr.service_id);
    hdr.instance_id        = lwip_ntohs(hdr.instance_id);
    hdr.method_or_event_id = lwip_ntohs(hdr.method_or_event_id);
    hdr.client_id          = lwip_ntohs(hdr.client_id);
    hdr.session_id         = lwip_ntohs(hdr.session_id);
    hdr.flags              = lwip_ntohs(hdr.flags);
    hdr.payload_length     = lwip_ntohl(hdr.payload_length);

    if (hdr.version != kFrameVersion) { return false; }

    const std::size_t expected =
        kFrameHeaderSize + static_cast<std::size_t>(hdr.payload_length);
    if (length != expected) { return false; }

    msg_out.service_id         = hdr.service_id;
    msg_out.instance_id        = hdr.instance_id;
    msg_out.method_or_event_id = hdr.method_or_event_id;
    msg_out.client_id          = hdr.client_id;
    msg_out.session_id         = hdr.session_id;
    msg_out.is_event    = ((hdr.flags & kFlagIsEventMask)    != 0U);
    msg_out.is_reliable = ((hdr.flags & kFlagIsReliableMask) != 0U);
    msg_out.payload.clear();

    if (hdr.payload_length > 0U)
    {
        msg_out.payload.resize(static_cast<std::size_t>(hdr.payload_length));
        std::memcpy(msg_out.payload.data(),
                    data + kFrameHeaderSize,
                    static_cast<std::size_t>(hdr.payload_length));
    }

    kind_out = hdr.message_kind;
    return true;
}

}  // namespace

// ---- LwipUdpTransportAdapter -----------------------------------------------

LwipUdpTransportAdapter::LwipUdpTransportAdapter(
    const LwipUdpConfig& config) noexcept
    : config_(config)
{
}

LwipUdpTransportAdapter::~LwipUdpTransportAdapter()
{
    static_cast<void>(Shutdown());
}

TransportStatus LwipUdpTransportAdapter::Initialize()
{
    if (is_initialized_) { return TransportStatus::kSuccess; }

    pcb_ = udp_new();
    if (pcb_ == nullptr) { return TransportStatus::kTransmissionFailed; }

    const err_t bind_err = udp_bind(pcb_, IP_ADDR_ANY, config_.local_port);
    if (bind_err != ERR_OK)
    {
        udp_remove(pcb_);
        pcb_ = nullptr;
        return TransportStatus::kTransmissionFailed;
    }

    udp_recv(pcb_, &LwipUdpTransportAdapter::RecvCallback, this);
    is_initialized_ = true;

    if (handler_ != nullptr)
    {
        handler_->OnTransportAvailabilityChanged(true);
    }

    return TransportStatus::kSuccess;
}

TransportStatus LwipUdpTransportAdapter::Shutdown()
{
    if (!is_initialized_) { return TransportStatus::kSuccess; }

    if (pcb_ != nullptr)
    {
        udp_remove(pcb_);
        pcb_ = nullptr;
    }

    is_initialized_ = false;

    if (handler_ != nullptr)
    {
        handler_->OnTransportAvailabilityChanged(false);
    }

    return TransportStatus::kSuccess;
}

TransportStatus LwipUdpTransportAdapter::SendRequest(
    const TransportMessage& transport_message)
{
    return SendMessage(transport_message, kRequestMessageKind);
}

TransportStatus LwipUdpTransportAdapter::SendResponse(
    const TransportMessage& transport_message)
{
    return SendMessage(transport_message, kResponseMessageKind);
}

TransportStatus LwipUdpTransportAdapter::SendEvent(
    const TransportMessage& transport_message)
{
    return SendMessage(transport_message, kEventMessageKind);
}

void LwipUdpTransportAdapter::SetMessageHandler(
    TransportMessageHandlerInterface* message_handler) noexcept
{
    handler_ = message_handler;
}

TransportStatus LwipUdpTransportAdapter::SendMessage(
    const TransportMessage& transport_message,
    const std::uint16_t     message_kind)
{
    if (!is_initialized_) { return TransportStatus::kNotInitialized; }

    const std::vector<std::uint8_t> frame = EncodeFrame(transport_message, message_kind);

    pbuf* const p = pbuf_alloc(PBUF_TRANSPORT,
                               static_cast<std::uint16_t>(frame.size()),
                               PBUF_RAM);
    if (p == nullptr) { return TransportStatus::kTransmissionFailed; }

    std::memcpy(p->payload, frame.data(), frame.size());

    // Build IPv4 address without using IP4_ADDR() which expands C-style casts.
    // With LWIP_IPV6=0, ip_addr_t is a typedef for ip4_addr_t so .addr is direct.
    ip_addr_t dest {};
    dest.addr = lwip_htonl(config_.remote_ip_addr);

    const err_t send_err = udp_sendto(pcb_, p, &dest, config_.remote_port);
    pbuf_free(p);

    return (send_err == ERR_OK) ? TransportStatus::kSuccess
                                : TransportStatus::kTransmissionFailed;
}

void LwipUdpTransportAdapter::RecvCallback(
    void* const          arg,
    udp_pcb* const       /*pcb*/,
    pbuf* const          p,
    const ip_addr_t* const /*addr*/,
    const std::uint16_t    /*port*/)
{
    auto* const self = static_cast<LwipUdpTransportAdapter*>(arg);

    if ((p == nullptr) || (self == nullptr) || (self->handler_ == nullptr))
    {
        if (p != nullptr) { pbuf_free(p); }
        return;
    }

    // Linearise potentially chained pbuf into a flat buffer.
    const std::uint16_t total_len = p->tot_len;
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(total_len));
    const std::uint16_t copied = pbuf_copy_partial(
        p, buf.data(), total_len, 0U);

    pbuf_free(p);

    if (copied != total_len) { return; }

    TransportMessage msg {};
    std::uint16_t    kind {0U};

    if (!DecodeFrame(buf.data(), buf.size(), msg, kind)) { return; }

    static_cast<void>(kind);
    self->handler_->OnTransportMessageReceived(msg);
}

}  // namespace body_control::lighting::transport::lwip

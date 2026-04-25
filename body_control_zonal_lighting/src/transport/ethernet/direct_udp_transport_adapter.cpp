// Send-only POSIX UDP transport for forwarding lamp commands to a remote
// endpoint over a physical Ethernet interface (e.g., STM32 NUCLEO).
//
// connect() + send() lets the kernel pick the source interface and port from
// the routing table — no explicit local-port binding needed.  The receiver
// side is intentionally absent: the STM32 node does not send responses.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <memory>
#include <vector>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

namespace body_control::lighting::transport::ethernet
{

// Defined in ethernet_frame_adapter.cpp — same namespace, same library unit.
std::vector<std::uint8_t> EncodeTransportMessage(
    const TransportMessage& transport_message,
    std::uint16_t           message_kind);

namespace
{

constexpr std::uint16_t kRequestMessageKind  {1U};
constexpr std::uint16_t kResponseMessageKind {2U};
constexpr std::uint16_t kEventMessageKind    {3U};

class DirectUdpTransportAdapter final : public TransportAdapterInterface
{
public:
    explicit DirectUdpTransportAdapter(
        const char* const   remote_ip,
        const std::uint16_t remote_port) noexcept
        : remote_port_(remote_port)
        , socket_fd_(-1)
        , is_initialized_(false)
        , handler_(nullptr)
    {
        std::strncpy(remote_ip_, remote_ip, sizeof(remote_ip_) - 1U);
        remote_ip_[sizeof(remote_ip_) - 1U] = '\0';
    }

    ~DirectUdpTransportAdapter() override
    {
        static_cast<void>(Shutdown());
    }

    [[nodiscard]] TransportStatus Initialize() override
    {
        if (is_initialized_) { return TransportStatus::kSuccess; }

        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) { return TransportStatus::kTransmissionFailed; }

        sockaddr_in remote {};
        remote.sin_family = AF_INET;
        remote.sin_port   = htons(remote_port_);
        if (inet_pton(AF_INET, remote_ip_, &remote.sin_addr) != 1)
        {
            static_cast<void>(::close(socket_fd_));
            socket_fd_ = -1;
            return TransportStatus::kTransmissionFailed;
        }

        if (::connect(socket_fd_,
                      reinterpret_cast<const sockaddr*>(&remote),
                      static_cast<socklen_t>(sizeof(remote))) < 0)
        {
            static_cast<void>(::close(socket_fd_));
            socket_fd_ = -1;
            return TransportStatus::kTransmissionFailed;
        }

        is_initialized_ = true;
        if (handler_ != nullptr) { handler_->OnTransportAvailabilityChanged(true); }
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus Shutdown() override
    {
        if (!is_initialized_) { return TransportStatus::kSuccess; }
        static_cast<void>(::close(socket_fd_));
        socket_fd_      = -1;
        is_initialized_ = false;
        if (handler_ != nullptr) { handler_->OnTransportAvailabilityChanged(false); }
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus SendRequest(
        const TransportMessage& msg) override
    {
        return Send(msg, kRequestMessageKind);
    }

    [[nodiscard]] TransportStatus SendResponse(
        const TransportMessage& msg) override
    {
        return Send(msg, kResponseMessageKind);
    }

    [[nodiscard]] TransportStatus SendEvent(
        const TransportMessage& msg) override
    {
        return Send(msg, kEventMessageKind);
    }

    void SetMessageHandler(
        TransportMessageHandlerInterface* const handler) noexcept override
    {
        handler_ = handler;
    }

private:
    [[nodiscard]] TransportStatus Send(
        const TransportMessage& msg,
        const std::uint16_t     kind)
    {
        if (!is_initialized_) { return TransportStatus::kNotInitialized; }

        const std::vector<std::uint8_t> frame =
            EncodeTransportMessage(msg, kind);

        const ssize_t sent =
            ::send(socket_fd_, frame.data(), frame.size(), 0);

        return (sent == static_cast<ssize_t>(frame.size()))
            ? TransportStatus::kSuccess
            : TransportStatus::kTransmissionFailed;
    }

    char                              remote_ip_[16U];
    std::uint16_t                     remote_port_;
    int                               socket_fd_;
    bool                              is_initialized_;
    TransportMessageHandlerInterface* handler_;
};

}  // namespace

std::unique_ptr<TransportAdapterInterface>
CreateDirectUdpTransportAdapter(
    const char* const   remote_ip,
    const std::uint16_t remote_port)
{
    return std::make_unique<DirectUdpTransportAdapter>(remote_ip, remote_port);
}

}  // namespace body_control::lighting::transport::ethernet

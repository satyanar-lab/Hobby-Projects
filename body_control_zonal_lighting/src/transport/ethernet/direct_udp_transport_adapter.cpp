// Bidirectional POSIX UDP transport for communicating with a remote endpoint
// (e.g. STM32 NUCLEO) over a physical Ethernet interface.
//
// Sends lamp commands to the remote peer and receives events (LampStatusEvent,
// NodeHealthEvent) sent back by the peer.
//
// Socket setup: bind(INADDR_ANY, local_port) then connect(remote_ip, remote_port).
// After connect(), recv() receives only from the connected peer; send() uses
// the connected address via send().  A background thread runs recv() until Shutdown().

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

namespace body_control::lighting::transport::ethernet
{

// Defined in ethernet_frame_adapter.cpp — same namespace, same library unit.
std::vector<std::uint8_t> EncodeTransportMessage(
    const TransportMessage& transport_message,
    std::uint16_t           message_kind);

bool DecodeTransportMessage(
    const std::uint8_t* frame_data,
    std::size_t         frame_length,
    TransportMessage&   transport_message,
    std::uint16_t&      message_kind) noexcept;

namespace
{

constexpr std::uint16_t kRequestMessageKind  {1U};
constexpr std::uint16_t kResponseMessageKind {2U};
constexpr std::uint16_t kEventMessageKind    {3U};
constexpr std::size_t   kMaxFrameSize        {1500U};

class DirectUdpTransportAdapter final : public TransportAdapterInterface
{
public:
    DirectUdpTransportAdapter(
        const char* const   remote_ip,
        const std::uint16_t remote_port,
        const std::uint16_t local_port) noexcept
        : remote_port_(remote_port)
        , local_port_(local_port)
        , socket_fd_(-1)
        , is_initialized_(false)
        , handler_(nullptr)
        , shutdown_flag_(false)
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

        socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) { return TransportStatus::kTransmissionFailed; }

        // Bind to a fixed local port so the NUCLEO can address events back to us.
        sockaddr_in local {};
        local.sin_family      = AF_INET;
        local.sin_addr.s_addr = htonl(INADDR_ANY);
        local.sin_port        = htons(local_port_);
        if (::bind(socket_fd_,
                   reinterpret_cast<const sockaddr*>(&local),
                   static_cast<socklen_t>(sizeof(local))) < 0)
        {
            static_cast<void>(::close(socket_fd_));
            socket_fd_ = -1;
            return TransportStatus::kTransmissionFailed;
        }

        // connect() sets the default send destination and filters recv() to
        // only accept datagrams from this peer.
        sockaddr_in remote {};
        remote.sin_family = AF_INET;
        remote.sin_port   = htons(remote_port_);
        if (::inet_pton(AF_INET, remote_ip_, &remote.sin_addr) != 1)
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

        shutdown_flag_.store(false);
        rx_thread_ = std::thread(&DirectUdpTransportAdapter::ReceiveLoop, this);

        is_initialized_ = true;
        TransportMessageHandlerInterface* const h = handler_.load();
        if (h != nullptr) { h->OnTransportAvailabilityChanged(true); }
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus Shutdown() override
    {
        if (!is_initialized_) { return TransportStatus::kSuccess; }

        shutdown_flag_.store(true);
        // Closing the socket unblocks recv() in the receive thread.
        static_cast<void>(::close(socket_fd_));
        socket_fd_ = -1;

        if (rx_thread_.joinable()) { rx_thread_.join(); }

        is_initialized_ = false;
        TransportMessageHandlerInterface* const h = handler_.load();
        if (h != nullptr) { h->OnTransportAvailabilityChanged(false); }
        return TransportStatus::kSuccess;
    }

    [[nodiscard]] TransportStatus SendRequest(const TransportMessage& msg) override
    {
        return Send(msg, kRequestMessageKind);
    }

    [[nodiscard]] TransportStatus SendResponse(const TransportMessage& msg) override
    {
        return Send(msg, kResponseMessageKind);
    }

    [[nodiscard]] TransportStatus SendEvent(const TransportMessage& msg) override
    {
        return Send(msg, kEventMessageKind);
    }

    void SetMessageHandler(
        TransportMessageHandlerInterface* const handler) noexcept override
    {
        handler_.store(handler);
    }

private:
    [[nodiscard]] TransportStatus Send(
        const TransportMessage& msg,
        const std::uint16_t     kind)
    {
        if (!is_initialized_) { return TransportStatus::kNotInitialized; }

        const std::vector<std::uint8_t> frame = EncodeTransportMessage(msg, kind);
        const ssize_t sent = ::send(socket_fd_, frame.data(), frame.size(), 0);
        return (sent == static_cast<ssize_t>(frame.size()))
            ? TransportStatus::kSuccess
            : TransportStatus::kTransmissionFailed;
    }

    void ReceiveLoop()
    {
        std::array<std::uint8_t, kMaxFrameSize> buf {};

        while (!shutdown_flag_.load())
        {
            const ssize_t received =
                ::recv(socket_fd_, buf.data(), buf.size(), 0);

            if (received <= 0) { break; }

            TransportMessageHandlerInterface* const h = handler_.load();
            if (h == nullptr) { continue; }

            TransportMessage msg {};
            std::uint16_t    kind {0U};
            if (DecodeTransportMessage(
                    buf.data(),
                    static_cast<std::size_t>(received),
                    msg,
                    kind))
            {
                static_cast<void>(kind);
                h->OnTransportMessageReceived(msg);
            }
        }
    }

    char                                           remote_ip_[16U];
    std::uint16_t                                  remote_port_;
    std::uint16_t                                  local_port_;
    int                                            socket_fd_;
    bool                                           is_initialized_;
    std::atomic<TransportMessageHandlerInterface*> handler_;
    std::atomic<bool>                              shutdown_flag_;
    std::thread                                    rx_thread_;
};

}  // namespace

std::unique_ptr<TransportAdapterInterface>
CreateDirectUdpTransportAdapter(
    const char* const   remote_ip,
    const std::uint16_t remote_port,
    const std::uint16_t local_port)
{
    return std::make_unique<DirectUdpTransportAdapter>(
        remote_ip, remote_port, local_port);
}

}  // namespace body_control::lighting::transport::ethernet

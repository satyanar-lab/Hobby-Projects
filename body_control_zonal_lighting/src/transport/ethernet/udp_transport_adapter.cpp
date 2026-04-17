#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{
namespace ethernet
{

std::vector<std::uint8_t> EncodeTransportMessage(
    const TransportMessage& transport_message,
    std::uint16_t message_kind);

bool DecodeTransportMessage(
    const std::uint8_t* frame_data,
    std::size_t frame_length,
    TransportMessage& transport_message,
    std::uint16_t& message_kind) noexcept;

namespace
{

constexpr std::uint16_t kRequestMessageKind {1U};
constexpr std::uint16_t kResponseMessageKind {2U};
constexpr std::uint16_t kEventMessageKind {3U};

constexpr std::uint16_t kCentralZoneControllerPort {41000U};
constexpr std::uint16_t kRearLightingNodePort {41001U};
constexpr std::uint16_t kControllerOperatorPort {41002U};
constexpr std::uint16_t kOperatorClientPort {41003U};

struct UdpEndpointConfig
{
    std::uint16_t local_port;
    std::uint16_t remote_port;
};

class UdpTransportAdapter final : public TransportAdapterInterface
{
public:
    explicit UdpTransportAdapter(
        const UdpEndpointConfig& endpoint_config) noexcept
        : endpoint_config_(endpoint_config)
        , socket_fd_(-1)
        , is_initialized_(false)
        , receiver_thread_running_(false)
        , message_handler_(nullptr)
    {
    }

    ~UdpTransportAdapter() override
    {
        static_cast<void>(Shutdown());
    }

    TransportStatus Initialize() override
    {
        if (is_initialized_)
        {
            return TransportStatus::kSuccess;
        }

        socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);

        if (socket_fd_ < 0)
        {
            return TransportStatus::kTransmissionFailed;
        }

        int reuse_address_flag {1};
        static_cast<void>(setsockopt(
            socket_fd_,
            SOL_SOCKET,
            SO_REUSEADDR,
            &reuse_address_flag,
            sizeof(reuse_address_flag)));

        std::memset(&local_address_, 0, sizeof(local_address_));
        local_address_.sin_family = AF_INET;
        local_address_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        local_address_.sin_port = htons(endpoint_config_.local_port);

        if (bind(
                socket_fd_,
                reinterpret_cast<sockaddr*>(&local_address_),
                sizeof(local_address_)) < 0)
        {
            CloseSocket();
            return TransportStatus::kTransmissionFailed;
        }

        std::memset(&remote_address_, 0, sizeof(remote_address_));
        remote_address_.sin_family = AF_INET;
        remote_address_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        remote_address_.sin_port = htons(endpoint_config_.remote_port);

        is_initialized_ = true;
        receiver_thread_running_ = true;
        receiver_thread_ = std::thread(&UdpTransportAdapter::ReceiverLoop, this);

        if (message_handler_ != nullptr)
        {
            message_handler_->OnTransportAvailabilityChanged(true);
        }

        return TransportStatus::kSuccess;
    }

    TransportStatus Shutdown() override
    {
        if (!is_initialized_)
        {
            return TransportStatus::kSuccess;
        }

        receiver_thread_running_ = false;
        CloseSocket();

        if (receiver_thread_.joinable())
        {
            receiver_thread_.join();
        }

        is_initialized_ = false;

        if (message_handler_ != nullptr)
        {
            message_handler_->OnTransportAvailabilityChanged(false);
        }

        return TransportStatus::kSuccess;
    }

    TransportStatus SendRequest(
        const TransportMessage& transport_message) override
    {
        return SendMessage(transport_message, kRequestMessageKind);
    }

    TransportStatus SendResponse(
        const TransportMessage& transport_message) override
    {
        return SendMessage(transport_message, kResponseMessageKind);
    }

    TransportStatus SendEvent(
        const TransportMessage& transport_message) override
    {
        return SendMessage(transport_message, kEventMessageKind);
    }

    void SetMessageHandler(
        TransportMessageHandlerInterface* message_handler) noexcept override
    {
        message_handler_ = message_handler;
    }

private:
    void ReceiverLoop()
    {
        while (receiver_thread_running_)
        {
            std::uint8_t receive_buffer[1500U] {};
            sockaddr_in source_address {};
            socklen_t source_address_length = sizeof(source_address);

            const ssize_t received_length = recvfrom(
                socket_fd_,
                receive_buffer,
                sizeof(receive_buffer),
                0,
                reinterpret_cast<sockaddr*>(&source_address),
                &source_address_length);

            if (received_length <= 0)
            {
                if (!receiver_thread_running_)
                {
                    break;
                }

                continue;
            }

            if (message_handler_ == nullptr)
            {
                continue;
            }

            TransportMessage transport_message {};
            std::uint16_t message_kind {0U};

            const bool is_message_valid = DecodeTransportMessage(
                receive_buffer,
                static_cast<std::size_t>(received_length),
                transport_message,
                message_kind);

            if (!is_message_valid)
            {
                continue;
            }

            static_cast<void>(message_kind);
            message_handler_->OnTransportMessageReceived(transport_message);
        }
    }

    TransportStatus SendMessage(
        const TransportMessage& transport_message,
        const std::uint16_t message_kind)
    {
        if (!is_initialized_)
        {
            return TransportStatus::kNotInitialized;
        }

        const std::vector<std::uint8_t> frame_bytes =
            EncodeTransportMessage(transport_message, message_kind);

        const ssize_t transmitted_length = sendto(
            socket_fd_,
            frame_bytes.data(),
            frame_bytes.size(),
            0,
            reinterpret_cast<const sockaddr*>(&remote_address_),
            sizeof(remote_address_));

        if (transmitted_length !=
            static_cast<ssize_t>(frame_bytes.size()))
        {
            return TransportStatus::kTransmissionFailed;
        }

        return TransportStatus::kSuccess;
    }

    void CloseSocket() noexcept
    {
        if (socket_fd_ >= 0)
        {
            static_cast<void>(close(socket_fd_));
            socket_fd_ = -1;
        }
    }

    UdpEndpointConfig endpoint_config_;
    int socket_fd_;
    sockaddr_in local_address_;
    sockaddr_in remote_address_;
    std::atomic<bool> is_initialized_;
    std::atomic<bool> receiver_thread_running_;
    std::thread receiver_thread_;
    TransportMessageHandlerInterface* message_handler_;
};

}  // namespace

std::unique_ptr<TransportAdapterInterface>
CreateCentralZoneControllerUdpTransportAdapter()
{
    const UdpEndpointConfig endpoint_config {
        kCentralZoneControllerPort,
        kRearLightingNodePort};

    return std::make_unique<UdpTransportAdapter>(endpoint_config);
}

std::unique_ptr<TransportAdapterInterface>
CreateRearLightingNodeUdpTransportAdapter()
{
    const UdpEndpointConfig endpoint_config {
        kRearLightingNodePort,
        kCentralZoneControllerPort};

    return std::make_unique<UdpTransportAdapter>(endpoint_config);
}

std::unique_ptr<TransportAdapterInterface>
CreateControllerOperatorUdpTransportAdapter()
{
    const UdpEndpointConfig endpoint_config {
        kControllerOperatorPort,
        kOperatorClientPort};

    return std::make_unique<UdpTransportAdapter>(endpoint_config);
}

std::unique_ptr<TransportAdapterInterface>
CreateOperatorClientUdpTransportAdapter()
{
    const UdpEndpointConfig endpoint_config {
        kOperatorClientPort,
        kControllerOperatorPort};

    return std::make_unique<UdpTransportAdapter>(endpoint_config);
}

}  // namespace ethernet
}  // namespace transport
}  // namespace lighting
}  // namespace body_control
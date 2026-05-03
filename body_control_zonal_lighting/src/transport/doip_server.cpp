#include "body_control/lighting/transport/doip_server.hpp"

#include <array>
#include <cerrno>
#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "body_control/lighting/domain/uds_service_ids.hpp"

namespace body_control
{
namespace lighting
{
namespace transport
{

DoipServer::DoipServer(
    application::UdsRequestHandler& handler,
    const std::uint16_t             port) noexcept
    : handler_ {handler}
    , port_    {port}
{
}

DoipServer::~DoipServer()
{
    Stop();
}

void DoipServer::Start() noexcept
{
    if (running_.load()) { return; }

    server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0)
    {
        std::cerr << "[DoIP] socket() failed: " << std::strerror(errno) << '\n';
        return;
    }

    const int opt {1};
    static_cast<void>(::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR,
                                   &opt, static_cast<socklen_t>(sizeof(opt))));

    struct sockaddr_in addr {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port_);

    if (::bind(server_fd_,
               reinterpret_cast<const struct sockaddr*>(&addr),
               static_cast<socklen_t>(sizeof(addr))) < 0)
    {
        std::cerr << "[DoIP] bind() failed on port " << port_
                  << ": " << std::strerror(errno) << '\n';
        ::close(server_fd_);
        server_fd_ = -1;
        return;
    }

    if (::listen(server_fd_, 1) < 0)
    {
        std::cerr << "[DoIP] listen() failed: " << std::strerror(errno) << '\n';
        ::close(server_fd_);
        server_fd_ = -1;
        return;
    }

    running_.store(true);
    thread_ = std::thread {[this]() { ListenerThread(); }};
    std::cout << "[DoIP] server listening on TCP port " << port_ << '\n';
}

void DoipServer::Stop() noexcept
{
    if (!running_.exchange(false)) { return; }

    // Close server socket to unblock accept().
    if (server_fd_ >= 0)
    {
        ::shutdown(server_fd_, SHUT_RDWR);
        ::close(server_fd_);
        server_fd_ = -1;
    }

    if (thread_.joinable()) { thread_.join(); }
}

// ────���────────────────────────────────────────────────────────────────────────

void DoipServer::ListenerThread() noexcept
{
    while (running_.load())
    {
        struct sockaddr_in client_addr {};
        socklen_t          addr_len {static_cast<socklen_t>(sizeof(client_addr))};

        const int conn_fd = ::accept(
            server_fd_,
            reinterpret_cast<struct sockaddr*>(&client_addr),
            &addr_len);

        if (conn_fd < 0)
        {
            if (!running_.load()) { break; }
            continue;
        }

        HandleConnection(conn_fd);
        ::close(conn_fd);
    }
}

void DoipServer::HandleConnection(const int conn_fd) noexcept
{
    bool routing_activated {false};

    while (running_.load())
    {
        // Read 8-byte DoIP generic header.
        std::array<std::uint8_t, kHeaderSize> hdr {};
        if (!RecvFull(conn_fd, hdr.data(), kHeaderSize)) { break; }

        if (hdr[0] != kProtocolVersion)
        {
            // Unsupported protocol version — close connection.
            break;
        }

        const std::uint16_t payload_type =
            static_cast<std::uint16_t>(
                (static_cast<std::uint16_t>(hdr[2]) << 8U) | hdr[3]);

        const std::uint32_t payload_len =
            (static_cast<std::uint32_t>(hdr[4]) << 24U) |
            (static_cast<std::uint32_t>(hdr[5]) << 16U) |
            (static_cast<std::uint32_t>(hdr[6]) << 8U)  |
            static_cast<std::uint32_t>(hdr[7]);

        if (payload_len > kMaxPayloadLength)
        {
            // Payload too large — reject to prevent OOM.
            break;
        }

        std::vector<std::uint8_t> payload(payload_len);
        if (payload_len > 0U &&
            !RecvFull(conn_fd, payload.data(), static_cast<std::size_t>(payload_len)))
        {
            break;
        }

        switch (payload_type)
        {
        case kTypeRoutingActivationReq:
            if (!HandleRoutingActivation(conn_fd, payload.data(), payload_len))
            {
                return;
            }
            routing_activated = true;
            break;

        case kTypeDiagnosticMessage:
            if (routing_activated)
            {
                HandleDiagnosticMessage(conn_fd, payload.data(), payload_len);
            }
            break;

        case kTypeAliveCheckResponse:
            break;  // consume silently

        default:
            break;  // ignore unsupported payload types
        }
    }
}

// ── Frame handlers ────────────────────────────────────────────────────────────

bool DoipServer::HandleRoutingActivation(
    const int            conn_fd,
    const std::uint8_t*  payload,
    const std::uint32_t  length) noexcept
{
    if (length < 7U) { return false; }

    const std::uint16_t tester_addr =
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[0]) << 8U) | payload[1]);

    // Response: tester_addr(2) + entity_addr(2) + response_code(1) + reserved(4)
    std::vector<std::uint8_t> resp(9U, 0x00U);
    resp[0] = static_cast<std::uint8_t>(tester_addr >> 8U);
    resp[1] = static_cast<std::uint8_t>(tester_addr & 0xFFU);
    resp[2] = static_cast<std::uint8_t>(
        domain::uds::kDoipRearNodeAddress >> 8U);
    resp[3] = static_cast<std::uint8_t>(
        domain::uds::kDoipRearNodeAddress & 0xFFU);
    resp[4] = kRoutingActivated;

    return SendAll(conn_fd, BuildFrame(kTypeRoutingActivationResp, resp));
}

void DoipServer::HandleDiagnosticMessage(
    const int            conn_fd,
    const std::uint8_t*  payload,
    const std::uint32_t  length) noexcept
{
    // Minimum: src(2) + target(2) + at least 1 UDS byte.
    if (length < 5U) { return; }

    const std::uint16_t src_addr =
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[0]) << 8U) | payload[1]);
    const std::uint16_t target_addr =
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(payload[2]) << 8U) | payload[3]);

    if (target_addr != domain::uds::kDoipRearNodeAddress) { return; }

    // Extract UDS request bytes.
    const std::vector<std::uint8_t> uds_request(
        payload + 4U, payload + static_cast<std::size_t>(length));

    // Send diagnostic message positive ACK: ECU_addr(2) + tester_addr(2) + ACK(1)
    std::vector<std::uint8_t> ack_payload(5U, 0x00U);
    ack_payload[0] = static_cast<std::uint8_t>(
        domain::uds::kDoipRearNodeAddress >> 8U);
    ack_payload[1] = static_cast<std::uint8_t>(
        domain::uds::kDoipRearNodeAddress & 0xFFU);
    ack_payload[2] = static_cast<std::uint8_t>(src_addr >> 8U);
    ack_payload[3] = static_cast<std::uint8_t>(src_addr & 0xFFU);
    ack_payload[4] = kDiagAckPositive;

    if (!SendAll(conn_fd, BuildFrame(kTypeDiagnosticMessageAck, ack_payload)))
    {
        return;
    }

    // Process UDS request and send response as a diagnostic message.
    const std::vector<std::uint8_t> uds_response =
        handler_.HandleRequest(uds_request);

    std::vector<std::uint8_t> resp_payload;
    resp_payload.reserve(4U + uds_response.size());
    resp_payload.push_back(static_cast<std::uint8_t>(
        domain::uds::kDoipRearNodeAddress >> 8U));
    resp_payload.push_back(static_cast<std::uint8_t>(
        domain::uds::kDoipRearNodeAddress & 0xFFU));
    resp_payload.push_back(static_cast<std::uint8_t>(src_addr >> 8U));
    resp_payload.push_back(static_cast<std::uint8_t>(src_addr & 0xFFU));
    resp_payload.insert(
        resp_payload.end(), uds_response.begin(), uds_response.end());

    SendAll(conn_fd, BuildFrame(kTypeDiagnosticMessage, resp_payload));
}

// ── Static helpers ────────────────────────────────────────────────────────────

std::vector<std::uint8_t> DoipServer::BuildFrame(
    const std::uint16_t                    payload_type,
    const std::vector<std::uint8_t>&       payload) noexcept
{
    const auto len = static_cast<std::uint32_t>(payload.size());

    std::vector<std::uint8_t> frame;
    frame.reserve(kHeaderSize + payload.size());

    frame.push_back(kProtocolVersion);
    frame.push_back(kInverseVersion);
    frame.push_back(static_cast<std::uint8_t>(payload_type >> 8U));
    frame.push_back(static_cast<std::uint8_t>(payload_type & 0xFFU));
    frame.push_back(static_cast<std::uint8_t>(len >> 24U));
    frame.push_back(static_cast<std::uint8_t>(len >> 16U));
    frame.push_back(static_cast<std::uint8_t>(len >> 8U));
    frame.push_back(static_cast<std::uint8_t>(len & 0xFFU));
    frame.insert(frame.end(), payload.begin(), payload.end());

    return frame;
}

bool DoipServer::RecvFull(
    const int     fd,
    std::uint8_t* buf,
    const std::size_t n) noexcept
{
    std::size_t received {0U};
    while (received < n)
    {
        const ssize_t r = ::recv(fd, buf + received, n - received, 0);
        if (r <= 0) { return false; }
        received += static_cast<std::size_t>(r);
    }
    return true;
}

bool DoipServer::SendAll(
    const int                        fd,
    const std::vector<std::uint8_t>& data) noexcept
{
    std::size_t sent {0U};
    const std::size_t total {data.size()};
    while (sent < total)
    {
        const ssize_t s = ::send(fd, data.data() + sent, total - sent, 0);
        if (s <= 0) { return false; }
        sent += static_cast<std::size_t>(s);
    }
    return true;
}

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

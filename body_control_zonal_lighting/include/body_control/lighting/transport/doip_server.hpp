#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <vector>

#include "body_control/lighting/application/uds_request_handler.hpp"

// DoIP server — ISO 13400-2 over TCP.
//
// Listens on TCP port 13400 and provides a minimal DoIP server for external
// UDS diagnostic tools (e.g. tools/uds_client/bcl_diagnostic_client.py).
//
// Supported payload types:
//   0x0005  Routing activation request  → 0x0006 response
//   0x8001  Diagnostic message          → 0x8002 ACK + 0x8001 UDS response
//   0x0008  Alive check response        → (consume silently)
//
// Omitted for this portfolio phase: vehicle announcement (0x0001–0x0004),
// TLS, multiple concurrent testers, session timeouts.  The server accepts
// one tester connection at a time; the tester must send routing activation
// before any diagnostic message is processed.

namespace body_control
{
namespace lighting
{
namespace transport
{

class DoipServer
{
public:
    // Construct with the UDS handler that will process incoming requests.
    // port defaults to the standard DoIP TCP diagnostic port (13400).
    explicit DoipServer(
        application::UdsRequestHandler& handler,
        std::uint16_t                   port = 13400U) noexcept;

    ~DoipServer();

    // Bind the server socket and start the listener thread.
    void Start() noexcept;

    // Signal the listener thread to stop and join it.
    void Stop() noexcept;

private:
    void ListenerThread() noexcept;
    void HandleConnection(int conn_fd) noexcept;

    bool HandleRoutingActivation(
        int                        conn_fd,
        const std::uint8_t*        payload,
        std::uint32_t              length) noexcept;

    void HandleDiagnosticMessage(
        int                        conn_fd,
        const std::uint8_t*        payload,
        std::uint32_t              length) noexcept;

    // Build a complete DoIP frame (8-byte header + payload).
    static std::vector<std::uint8_t> BuildFrame(
        std::uint16_t                        payload_type,
        const std::vector<std::uint8_t>&     payload) noexcept;

    // Blocking receive — returns false on EOF or error.
    static bool RecvFull(int fd, std::uint8_t* buf, std::size_t n) noexcept;

    // Blocking send — returns false on error.
    static bool SendAll(
        int                              fd,
        const std::vector<std::uint8_t>& data) noexcept;

    // DoIP protocol constants (ISO 13400-2:2019).
    static constexpr std::uint8_t  kProtocolVersion    {0xFDU};
    static constexpr std::uint8_t  kInverseVersion     {0x02U};
    static constexpr std::size_t   kHeaderSize         {8U};
    static constexpr std::uint32_t kMaxPayloadLength   {65536U};  // sanity cap

    static constexpr std::uint16_t kTypeRoutingActivationReq  {0x0005U};
    static constexpr std::uint16_t kTypeRoutingActivationResp {0x0006U};
    static constexpr std::uint16_t kTypeDiagnosticMessage     {0x8001U};
    static constexpr std::uint16_t kTypeDiagnosticMessageAck  {0x8002U};
    static constexpr std::uint16_t kTypeAliveCheckResponse    {0x0008U};

    // Routing activation response code: routing successfully activated.
    static constexpr std::uint8_t  kRoutingActivated           {0x10U};
    // Diagnostic message positive ACK code.
    static constexpr std::uint8_t  kDiagAckPositive            {0x00U};

    application::UdsRequestHandler& handler_;
    std::uint16_t                   port_;
    int                             server_fd_  {-1};
    std::atomic<bool>               running_    {false};
    std::thread                     thread_;
};

}  // namespace transport
}  // namespace lighting
}  // namespace body_control

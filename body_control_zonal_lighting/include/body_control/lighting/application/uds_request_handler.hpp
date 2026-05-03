#pragma once

#include <cstdint>
#include <vector>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"

// UDS request handler for the body control rear lighting node.
//
// Implements the UDS services exposed over the DoIP diagnostic channel
// (ISO 14229-1 + ISO 13400-2).  All encoding is big-endian to match the
// UDS wire format.
//
// Thread safety: HandleRequest is called from the DoIP server thread while
// the SOME/IP service callbacks run on the vsomeip dispatch thread and the
// main loop runs on the main thread — all three access the shared
// RearLightingFunctionManager concurrently.  For this portfolio demo the
// race is benign (DID reads see at worst one-cycle-stale data; fault
// inject/clear from UDS is a low-frequency test operation).  In production
// these would be serialised through an application-level command queue.

namespace body_control
{
namespace lighting
{
namespace application
{

class UdsRequestHandler
{
public:
    explicit UdsRequestHandler(
        RearLightingFunctionManager& function_manager) noexcept;

    // Dispatch a raw UDS request and return the complete UDS response bytes.
    // Unknown or malformed requests return a negative response (0x7F).
    [[nodiscard]] std::vector<std::uint8_t> HandleRequest(
        const std::vector<std::uint8_t>& uds_request) noexcept;

private:
    std::vector<std::uint8_t> HandleDiagnosticSessionControl(
        const std::vector<std::uint8_t>& req) noexcept;

    std::vector<std::uint8_t> HandleReadDataByIdentifier(
        const std::vector<std::uint8_t>& req) noexcept;

    std::vector<std::uint8_t> HandleClearDiagnosticInformation(
        const std::vector<std::uint8_t>& req) noexcept;

    std::vector<std::uint8_t> HandleRoutineControl(
        const std::vector<std::uint8_t>& req) noexcept;

    std::vector<std::uint8_t> HandleReadDtcInformation(
        const std::vector<std::uint8_t>& req) noexcept;

    // DID encoders — return the DID payload bytes (without the 0x62 prefix
    // or DID bytes; caller prepends those).
    std::vector<std::uint8_t> EncodeLampStatus() const noexcept;
    std::vector<std::uint8_t> EncodeNodeHealth() const noexcept;
    std::vector<std::uint8_t> EncodeActiveFaults() const noexcept;
    static std::vector<std::uint8_t> EncodeEcuIdentification() noexcept;

    // Build a UDS negative response: [0x7F, request_sid, nrc].
    static std::vector<std::uint8_t> NegativeResponse(
        std::uint8_t request_sid, std::uint8_t nrc) noexcept;

    // Encode a DTC as 3 big-endian bytes from a uint16 FaultCode value.
    // High byte is always 0x00 since our codes fit in 16 bits.
    static void AppendDtc3Bytes(
        std::vector<std::uint8_t>& out,
        std::uint16_t fault_code_value) noexcept;

    RearLightingFunctionManager& function_manager_;
};

}  // namespace application
}  // namespace lighting
}  // namespace body_control

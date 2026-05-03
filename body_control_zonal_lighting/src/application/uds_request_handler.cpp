#include "body_control/lighting/application/uds_request_handler.hpp"

#include <array>
#include <cstdint>
#include <cstring>

#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/uds_service_ids.hpp"

namespace body_control
{
namespace lighting
{
namespace application
{

namespace
{

// All five lamp functions in declaration order — used for DID encoders that
// iterate the complete function set.
constexpr std::array<domain::LampFunction, 5U> kAllLampFunctions {
    domain::LampFunction::kLeftIndicator,
    domain::LampFunction::kRightIndicator,
    domain::LampFunction::kHazardLamp,
    domain::LampFunction::kParkLamp,
    domain::LampFunction::kHeadLamp
};

// Map a UDS RoutineControl routine identifier back to a LampFunction.
// Routine IDs mirror FaultCode values (0xB001–0xB005).
domain::LampFunction RoutineIdToLampFunction(const std::uint16_t id) noexcept
{
    switch (id)
    {
    case 0xB001U: return domain::LampFunction::kLeftIndicator;
    case 0xB002U: return domain::LampFunction::kRightIndicator;
    case 0xB003U: return domain::LampFunction::kHazardLamp;
    case 0xB004U: return domain::LampFunction::kParkLamp;
    case 0xB005U: return domain::LampFunction::kHeadLamp;
    default:      return domain::LampFunction::kUnknown;
    }
}

}  // namespace

// ─────────────────────────────────────────────────────────────────────────────

UdsRequestHandler::UdsRequestHandler(
    RearLightingFunctionManager& function_manager) noexcept
    : function_manager_ {function_manager}
{
}

std::vector<std::uint8_t> UdsRequestHandler::HandleRequest(
    const std::vector<std::uint8_t>& uds_request) noexcept
{
    if (uds_request.empty())
    {
        return NegativeResponse(0x00U, domain::uds::kNrcServiceNotSupported);
    }

    const std::uint8_t sid = uds_request[0];

    switch (sid)
    {
    case domain::uds::kSidDiagnosticSessionControl:
        return HandleDiagnosticSessionControl(uds_request);

    case domain::uds::kSidReadDataByIdentifier:
        return HandleReadDataByIdentifier(uds_request);

    case domain::uds::kSidClearDiagnosticInformation:
        return HandleClearDiagnosticInformation(uds_request);

    case domain::uds::kSidRoutineControl:
        return HandleRoutineControl(uds_request);

    case domain::uds::kSidReadDtcInformation:
        return HandleReadDtcInformation(uds_request);

    default:
        return NegativeResponse(sid, domain::uds::kNrcServiceNotSupported);
    }
}

// ── 0x10 DiagnosticSessionControl ────────────────────────────────────────────

std::vector<std::uint8_t> UdsRequestHandler::HandleDiagnosticSessionControl(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (req.size() < 2U)
    {
        return NegativeResponse(domain::uds::kSidDiagnosticSessionControl,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const std::uint8_t session = req[1] & 0x7FU;  // strip suppress-positive-response bit

    if (session != domain::uds::kSessionDefault &&
        session != domain::uds::kSessionExtended)
    {
        return NegativeResponse(domain::uds::kSidDiagnosticSessionControl,
                                domain::uds::kNrcSubFunctionNotSupported);
    }

    // Positive response: 0x50 + session + P2_server (25 ms) + P2*_server (500 ms)
    return {
        static_cast<std::uint8_t>(domain::uds::kSidDiagnosticSessionControl +
                                  domain::uds::kPositiveResponseOffset),
        session,
        0x00U, 0x19U,   // P2_server_max = 25 ms
        0x01U, 0xF4U    // P2*_server_max = 500 ms
    };
}

// ── 0x22 ReadDataByIdentifier ─────────────────────────────────────────────────

std::vector<std::uint8_t> UdsRequestHandler::HandleReadDataByIdentifier(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (req.size() < 3U)
    {
        return NegativeResponse(domain::uds::kSidReadDataByIdentifier,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const std::uint16_t did =
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(req[1]) << 8U) | req[2]);

    std::vector<std::uint8_t> payload_data;

    switch (did)
    {
    case domain::uds::kDidEcuIdentification:
        payload_data = EncodeEcuIdentification();
        break;
    case domain::uds::kDidLampStatus:
        payload_data = EncodeLampStatus();
        break;
    case domain::uds::kDidNodeHealth:
        payload_data = EncodeNodeHealth();
        break;
    case domain::uds::kDidActiveFaults:
        payload_data = EncodeActiveFaults();
        break;
    default:
        return NegativeResponse(domain::uds::kSidReadDataByIdentifier,
                                domain::uds::kNrcRequestOutOfRange);
    }

    // Positive response: 0x62 + DID bytes + data
    std::vector<std::uint8_t> response;
    response.reserve(3U + payload_data.size());
    response.push_back(static_cast<std::uint8_t>(
        domain::uds::kSidReadDataByIdentifier +
        domain::uds::kPositiveResponseOffset));
    response.push_back(req[1]);
    response.push_back(req[2]);
    response.insert(response.end(), payload_data.begin(), payload_data.end());
    return response;
}

// ── 0x14 ClearDiagnosticInformation ──────────────────────────────────────────

std::vector<std::uint8_t> UdsRequestHandler::HandleClearDiagnosticInformation(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (req.size() < 4U)
    {
        return NegativeResponse(domain::uds::kSidClearDiagnosticInformation,
                                domain::uds::kNrcRequestOutOfRange);
    }

    // Accept group 0xFFFFFF (all DTCs) only.
    const std::uint32_t group =
        (static_cast<std::uint32_t>(req[1]) << 16U) |
        (static_cast<std::uint32_t>(req[2]) << 8U)  |
        static_cast<std::uint32_t>(req[3]);

    if (group != domain::uds::kDtcGroupAll)
    {
        return NegativeResponse(domain::uds::kSidClearDiagnosticInformation,
                                domain::uds::kNrcRequestOutOfRange);
    }

    function_manager_.HandleClearAllFaults();

    return {static_cast<std::uint8_t>(
        domain::uds::kSidClearDiagnosticInformation +
        domain::uds::kPositiveResponseOffset)};
}

// ── 0x31 RoutineControl ───────────────────────────────────────────────────────

std::vector<std::uint8_t> UdsRequestHandler::HandleRoutineControl(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (req.size() < 4U)
    {
        return NegativeResponse(domain::uds::kSidRoutineControl,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const std::uint8_t  sub_func    = req[1];
    const std::uint16_t routine_id  =
        static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(req[2]) << 8U) | req[3]);

    if (sub_func != domain::uds::kRoutineStart &&
        sub_func != domain::uds::kRoutineStop)
    {
        return NegativeResponse(domain::uds::kSidRoutineControl,
                                domain::uds::kNrcSubFunctionNotSupported);
    }

    if (routine_id < domain::uds::kRoutineIdFaultBase ||
        routine_id > domain::uds::kRoutineIdFaultMax)
    {
        return NegativeResponse(domain::uds::kSidRoutineControl,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const domain::LampFunction fn = RoutineIdToLampFunction(routine_id);

    if (fn == domain::LampFunction::kUnknown)
    {
        return NegativeResponse(domain::uds::kSidRoutineControl,
                                domain::uds::kNrcRequestOutOfRange);
    }

    if (sub_func == domain::uds::kRoutineStart)
    {
        function_manager_.HandleFaultInjection(fn);
    }
    else
    {
        function_manager_.HandleFaultClear(fn);
    }

    // Positive response: 0x71 + sub_func + routine_id_hi + routine_id_lo
    return {
        static_cast<std::uint8_t>(domain::uds::kSidRoutineControl +
                                  domain::uds::kPositiveResponseOffset),
        sub_func,
        req[2],
        req[3]
    };
}

// ── 0x19 ReadDTCInformation ───────────────────────────────────────────────────

std::vector<std::uint8_t> UdsRequestHandler::HandleReadDtcInformation(
    const std::vector<std::uint8_t>& req) noexcept
{
    if (req.size() < 2U)
    {
        return NegativeResponse(domain::uds::kSidReadDtcInformation,
                                domain::uds::kNrcRequestOutOfRange);
    }

    const std::uint8_t sub_func = req[1];

    if (sub_func != domain::uds::kDtcSubReportByStatusMask &&
        sub_func != domain::uds::kDtcSubReportSupported)
    {
        return NegativeResponse(domain::uds::kSidReadDtcInformation,
                                domain::uds::kNrcSubFunctionNotSupported);
    }

    const domain::LampFaultStatus fault_status =
        function_manager_.GetFaultStatus();

    // Positive response header: 0x59 + sub_func + status_availability_mask
    std::vector<std::uint8_t> response;
    response.push_back(static_cast<std::uint8_t>(
        domain::uds::kSidReadDtcInformation + domain::uds::kPositiveResponseOffset));
    response.push_back(sub_func);
    response.push_back(domain::uds::kDtcStatusAvailabilityMask);

    if (sub_func == domain::uds::kDtcSubReportByStatusMask)
    {
        // Report active faults only.
        for (std::size_t i = 0U; i < fault_status.active_fault_count; ++i)
        {
            const auto code_val =
                static_cast<std::uint16_t>(fault_status.active_faults[i]);
            AppendDtc3Bytes(response, code_val);
            response.push_back(domain::uds::kDtcStatusActive);
        }
    }
    else
    {
        // kDtcSubReportSupported — list every fault code this node can ever raise.
        for (const domain::LampFunction fn : kAllLampFunctions)
        {
            const auto code_val =
                static_cast<std::uint16_t>(domain::LampFunctionToFaultCode(fn));
            AppendDtc3Bytes(response, code_val);
            // status: 0x08 = confirmedDTC (supported but not necessarily active)
            response.push_back(0x08U);
        }
    }

    return response;
}

// ── DID encoders ──────────────────────────────────────────────────────────────

std::vector<std::uint8_t> UdsRequestHandler::EncodeLampStatus() const noexcept
{
    std::vector<std::uint8_t> result;
    result.reserve(5U);

    for (const domain::LampFunction fn : kAllLampFunctions)
    {
        domain::LampStatus status {};
        function_manager_.GetLampStatus(fn, status);
        result.push_back(static_cast<std::uint8_t>(status.output_state));
    }

    return result;
}

std::vector<std::uint8_t> UdsRequestHandler::EncodeNodeHealth() const noexcept
{
    const domain::LampFaultStatus fault_status =
        function_manager_.GetFaultStatus();

    // Derive health state from fault count — if UDS is reachable the link is up.
    const std::uint8_t health_state =
        (fault_status.active_fault_count > 0U)
            ? 0x03U   // kFaulted
            : 0x01U;  // kOperational

    // Flags: bit0=eth_link(assumed up), bit1=svc_avail(assumed up), bit2=fault_present
    const std::uint8_t fault_bit =
        fault_status.fault_present ? 0x04U : 0x00U;
    const std::uint8_t flags =
        static_cast<std::uint8_t>(0x03U | fault_bit);  // eth + svc always up

    return {
        health_state,
        flags,
        fault_status.active_fault_count
    };
}

std::vector<std::uint8_t> UdsRequestHandler::EncodeActiveFaults() const noexcept
{
    const domain::LampFaultStatus fault_status =
        function_manager_.GetFaultStatus();

    std::vector<std::uint8_t> result;
    result.push_back(fault_status.active_fault_count);

    for (std::size_t i = 0U; i < fault_status.active_fault_count; ++i)
    {
        const auto code_val =
            static_cast<std::uint16_t>(fault_status.active_faults[i]);
        result.push_back(static_cast<std::uint8_t>(code_val >> 8U));
        result.push_back(static_cast<std::uint8_t>(code_val & 0xFFU));
    }

    return result;
}

std::vector<std::uint8_t> UdsRequestHandler::EncodeEcuIdentification() noexcept
{
    // Return an ASCII ECU identification string.  The version string is
    // injected via the project-wide compile definition set in CMakeLists.
    static const char kEcuId[] {"BCL-REAR-NODE " BODY_CONTROL_LIGHTING_PROJECT_VERSION};
    const std::size_t len = std::strlen(kEcuId);
    return std::vector<std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(kEcuId),
        reinterpret_cast<const std::uint8_t*>(kEcuId) + len);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

std::vector<std::uint8_t> UdsRequestHandler::NegativeResponse(
    const std::uint8_t request_sid, const std::uint8_t nrc) noexcept
{
    return {domain::uds::kSidNegativeResponse, request_sid, nrc};
}

void UdsRequestHandler::AppendDtc3Bytes(
    std::vector<std::uint8_t>& out,
    const std::uint16_t fault_code_value) noexcept
{
    out.push_back(0x00U);  // our fault codes fit in 16 bits; high byte = 0
    out.push_back(static_cast<std::uint8_t>(fault_code_value >> 8U));
    out.push_back(static_cast<std::uint8_t>(fault_code_value & 0xFFU));
}

}  // namespace application
}  // namespace lighting
}  // namespace body_control

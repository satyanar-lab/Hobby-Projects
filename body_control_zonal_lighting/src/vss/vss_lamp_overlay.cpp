#include "vss_lamp_overlay.hpp"

#include <sstream>

// Generated at build time by vss/generate_header.py via CMake target bcl_vss_generate.
// Provides constexpr std::string_view path constants in body_control::lighting::vss::paths.
// Include here (not in the header) to confine the build-output dependency to this TU.
#include "vss/bcl_vss_paths.hpp"

namespace body_control::lighting::vss {

namespace {

[[nodiscard]] bool IsLampOn(
    const std::array<domain::LampStatus, VssLampOverlay::kLampFunctionCount>& statuses,
    domain::LampFunction target) noexcept {
    for (const auto& status : statuses) {
        // cppcheck-suppress useStlAlgorithm
        if (status.function == target) {
            return status.output_state == domain::LampOutputState::kOn;
        }
    }
    return false;
}

[[nodiscard]] std::uint16_t FindFaultCode(
    const domain::LampFaultStatus& fault_status,
    domain::FaultCode target) noexcept {
    const std::size_t count {
        static_cast<std::size_t>(fault_status.active_fault_count)};
    for (std::size_t i = 0U; i < count; ++i) {
        if (fault_status.active_faults[i] == target) {
            return static_cast<std::uint16_t>(target);
        }
    }
    return 0U;
}

[[nodiscard]] std::uint16_t ExtractSequenceCounter(
    const std::array<domain::LampStatus, VssLampOverlay::kLampFunctionCount>& statuses) noexcept {
    for (const auto& status : statuses) {
        // cppcheck-suppress useStlAlgorithm
        if (status.function != domain::LampFunction::kUnknown) {
            return status.last_sequence_counter;
        }
    }
    return 0U;
}

}  // namespace

VssSnapshot VssLampOverlay::Snapshot(
    const std::array<domain::LampStatus, kLampFunctionCount>& lamp_statuses,
    const domain::LampFaultStatus& fault_status) const noexcept {
    VssSnapshot snap;

    snap.beam_low_is_on =
        IsLampOn(lamp_statuses, domain::LampFunction::kHeadLamp);
    snap.direction_indicator_left_is_signaling =
        IsLampOn(lamp_statuses, domain::LampFunction::kLeftIndicator);
    snap.direction_indicator_right_is_signaling =
        IsLampOn(lamp_statuses, domain::LampFunction::kRightIndicator);
    snap.hazard_is_signaling =
        IsLampOn(lamp_statuses, domain::LampFunction::kHazardLamp);
    snap.parking_is_on =
        IsLampOn(lamp_statuses, domain::LampFunction::kParkLamp);

    snap.bcl_command_sequence_counter =
        ExtractSequenceCounter(lamp_statuses);

    snap.bcl_hazard_active_fault_code =
        FindFaultCode(fault_status, domain::FaultCode::kHazardLamp);
    snap.bcl_head_lamp_active_fault_code =
        FindFaultCode(fault_status, domain::FaultCode::kHeadLamp);
    snap.bcl_left_indicator_active_fault_code =
        FindFaultCode(fault_status, domain::FaultCode::kLeftIndicator);
    snap.bcl_park_lamp_active_fault_code =
        FindFaultCode(fault_status, domain::FaultCode::kParkLamp);
    snap.bcl_right_indicator_active_fault_code =
        FindFaultCode(fault_status, domain::FaultCode::kRightIndicator);

    return snap;
}

std::string VssLampOverlay::ToJson(const VssSnapshot& snap) {
    namespace p = paths;
    std::ostringstream oss;

    auto b = [](bool v) -> std::string_view {
        return v ? std::string_view{"true"} : std::string_view{"false"};
    };

    oss << "{\n";
    oss << "  \"" << p::kVehicleBodyLightsBeamLowIsOn
        << "\": " << b(snap.beam_low_is_on) << ",\n";
    oss << "  \"" << p::kVehicleBodyLightsDirectionIndicatorLeftIsSignaling
        << "\": " << b(snap.direction_indicator_left_is_signaling) << ",\n";
    oss << "  \"" << p::kVehicleBodyLightsDirectionIndicatorRightIsSignaling
        << "\": " << b(snap.direction_indicator_right_is_signaling) << ",\n";
    oss << "  \"" << p::kVehicleBodyLightsHazardIsSignaling
        << "\": " << b(snap.hazard_is_signaling) << ",\n";
    oss << "  \"" << p::kVehicleBodyLightsParkingIsOn
        << "\": " << b(snap.parking_is_on) << ",\n";
    oss << "  \"" << p::kVehiclePrivateBCLLightingCommandSequenceCounter
        << "\": " << static_cast<std::uint32_t>(snap.bcl_command_sequence_counter) << ",\n";
    oss << "  \"" << p::kVehiclePrivateBCLLightingHazardActiveFaultCode
        << "\": " << static_cast<std::uint32_t>(snap.bcl_hazard_active_fault_code) << ",\n";
    oss << "  \"" << p::kVehiclePrivateBCLLightingHeadLampActiveFaultCode
        << "\": " << static_cast<std::uint32_t>(snap.bcl_head_lamp_active_fault_code) << ",\n";
    oss << "  \"" << p::kVehiclePrivateBCLLightingLeftIndicatorActiveFaultCode
        << "\": " << static_cast<std::uint32_t>(snap.bcl_left_indicator_active_fault_code) << ",\n";
    oss << "  \"" << p::kVehiclePrivateBCLLightingParkLampActiveFaultCode
        << "\": " << static_cast<std::uint32_t>(snap.bcl_park_lamp_active_fault_code) << ",\n";
    oss << "  \"" << p::kVehiclePrivateBCLLightingRightIndicatorActiveFaultCode
        << "\": " << static_cast<std::uint32_t>(snap.bcl_right_indicator_active_fault_code) << "\n";
    oss << "}";

    return oss.str();
}

}  // namespace body_control::lighting::vss

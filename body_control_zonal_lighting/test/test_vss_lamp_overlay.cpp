// Tests for VssLampOverlay — 7 tests covering the 5 lamp mappings, fault-code
// mapping, and JSON serialization shape.
//
// Test structure:
//   Tests 1–5: one lamp function per test; verify the correct bool field is
//     set and all other bool fields are false (cross-contamination guard).
//   Test 6: all 5 DTCs injected simultaneously; verify all five ActiveFaultCode
//     fields and CommandSequenceCounter pass through unchanged.
//   Test 7: serialize a mixed snapshot to JSON; verify all 11 keys are present,
//     appear in alphabetical order, and carry the expected values.
//
// Note: Tests 1–5 leave the non-targeted lamp slots at default (function ==
// kUnknown), so IsLampOn returns false for all unset functions without
// requiring a full array initialisation.

#include <array>
#include <cstdint>
#include <string>

#include <gtest/gtest.h>

#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "vss_lamp_overlay.hpp"

namespace bcl  = body_control::lighting;
namespace dom  = bcl::domain;
namespace vss  = bcl::vss;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static dom::LampStatus MakeLampStatus(dom::LampFunction func,
                                      bool on,
                                      std::uint16_t seq = 0U) {
    dom::LampStatus s;
    s.function     = func;
    s.output_state = on ? dom::LampOutputState::kOn : dom::LampOutputState::kOff;
    s.last_sequence_counter = seq;
    return s;
}

static std::array<dom::LampStatus, vss::VssLampOverlay::kLampFunctionCount>
EmptyLamps() {
    return {};  // all default-init: function=kUnknown, output_state=kUnknown
}

// ---------------------------------------------------------------------------
// Test 1: Left indicator
// ---------------------------------------------------------------------------

TEST(VssLampOverlayTest, LeftIndicatorMapping) {
    auto lamps = EmptyLamps();
    lamps[0] = MakeLampStatus(dom::LampFunction::kLeftIndicator, true);

    const dom::LampFaultStatus no_faults{};
    vss::VssLampOverlay overlay;
    const vss::VssSnapshot snap = overlay.Snapshot(lamps, no_faults);

    EXPECT_TRUE(snap.direction_indicator_left_is_signaling);
    EXPECT_FALSE(snap.direction_indicator_right_is_signaling);
    EXPECT_FALSE(snap.hazard_is_signaling);
    EXPECT_FALSE(snap.parking_is_on);
    EXPECT_FALSE(snap.beam_low_is_on);
}

// ---------------------------------------------------------------------------
// Test 2: Right indicator (symmetric to Test 1)
// ---------------------------------------------------------------------------

TEST(VssLampOverlayTest, RightIndicatorMapping) {
    auto lamps = EmptyLamps();
    lamps[0] = MakeLampStatus(dom::LampFunction::kRightIndicator, true);

    const dom::LampFaultStatus no_faults{};
    vss::VssLampOverlay overlay;
    const vss::VssSnapshot snap = overlay.Snapshot(lamps, no_faults);

    EXPECT_FALSE(snap.direction_indicator_left_is_signaling);
    EXPECT_TRUE(snap.direction_indicator_right_is_signaling);
    EXPECT_FALSE(snap.hazard_is_signaling);
    EXPECT_FALSE(snap.parking_is_on);
    EXPECT_FALSE(snap.beam_low_is_on);
}

// ---------------------------------------------------------------------------
// Test 3: Hazard
//
// The BCL domain models hazard as a separate LampFunction (kHazardLamp).
// The rear node drives its own GPIO for hazard; kLeftIndicator and
// kRightIndicator outputs are independent. A hazard-on command does not
// flip kLeftIndicator or kRightIndicator output states in the domain.
// Therefore only hazard_is_signaling is expected true.
// ---------------------------------------------------------------------------

TEST(VssLampOverlayTest, HazardMapping) {
    auto lamps = EmptyLamps();
    lamps[0] = MakeLampStatus(dom::LampFunction::kHazardLamp, true);

    const dom::LampFaultStatus no_faults{};
    vss::VssLampOverlay overlay;
    const vss::VssSnapshot snap = overlay.Snapshot(lamps, no_faults);

    EXPECT_FALSE(snap.direction_indicator_left_is_signaling);
    EXPECT_FALSE(snap.direction_indicator_right_is_signaling);
    EXPECT_TRUE(snap.hazard_is_signaling);
    EXPECT_FALSE(snap.parking_is_on);
    EXPECT_FALSE(snap.beam_low_is_on);
}

// ---------------------------------------------------------------------------
// Test 4: Parking lamp
// ---------------------------------------------------------------------------

TEST(VssLampOverlayTest, ParkingMapping) {
    auto lamps = EmptyLamps();
    lamps[0] = MakeLampStatus(dom::LampFunction::kParkLamp, true);

    const dom::LampFaultStatus no_faults{};
    vss::VssLampOverlay overlay;
    const vss::VssSnapshot snap = overlay.Snapshot(lamps, no_faults);

    EXPECT_FALSE(snap.direction_indicator_left_is_signaling);
    EXPECT_FALSE(snap.direction_indicator_right_is_signaling);
    EXPECT_FALSE(snap.hazard_is_signaling);
    EXPECT_TRUE(snap.parking_is_on);
    EXPECT_FALSE(snap.beam_low_is_on);
}

// ---------------------------------------------------------------------------
// Test 5: Headlamp → Beam.Low.IsOn
// ---------------------------------------------------------------------------

TEST(VssLampOverlayTest, HeadlampMapping) {
    auto lamps = EmptyLamps();
    lamps[0] = MakeLampStatus(dom::LampFunction::kHeadLamp, true);

    const dom::LampFaultStatus no_faults{};
    vss::VssLampOverlay overlay;
    const vss::VssSnapshot snap = overlay.Snapshot(lamps, no_faults);

    EXPECT_FALSE(snap.direction_indicator_left_is_signaling);
    EXPECT_FALSE(snap.direction_indicator_right_is_signaling);
    EXPECT_FALSE(snap.hazard_is_signaling);
    EXPECT_FALSE(snap.parking_is_on);
    EXPECT_TRUE(snap.beam_low_is_on);
}

// ---------------------------------------------------------------------------
// Test 6: Fault-code mapping + CommandSequenceCounter passthrough
//
// All five DTCs active simultaneously; each ActiveFaultCode field must match
// the corresponding FaultCode enum value. CommandSequenceCounter is taken
// from the first valid LampStatus entry.
// ---------------------------------------------------------------------------

TEST(VssLampOverlayTest, FaultCodeMappingAndSequenceCounter) {
    auto lamps = EmptyLamps();
    lamps[0] = MakeLampStatus(dom::LampFunction::kLeftIndicator,
                              false, /*seq=*/0xABCDU);

    dom::LampFaultStatus fault_status{};
    fault_status.fault_present      = true;
    fault_status.active_fault_count = 5U;
    fault_status.active_faults[0]   = dom::FaultCode::kLeftIndicator;   // 0xB001
    fault_status.active_faults[1]   = dom::FaultCode::kRightIndicator;  // 0xB002
    fault_status.active_faults[2]   = dom::FaultCode::kHazardLamp;      // 0xB003
    fault_status.active_faults[3]   = dom::FaultCode::kParkLamp;        // 0xB004
    fault_status.active_faults[4]   = dom::FaultCode::kHeadLamp;        // 0xB005

    vss::VssLampOverlay overlay;
    const vss::VssSnapshot snap = overlay.Snapshot(lamps, fault_status);

    EXPECT_EQ(snap.bcl_left_indicator_active_fault_code,  std::uint16_t{0xB001U});
    EXPECT_EQ(snap.bcl_right_indicator_active_fault_code, std::uint16_t{0xB002U});
    EXPECT_EQ(snap.bcl_hazard_active_fault_code,          std::uint16_t{0xB003U});
    EXPECT_EQ(snap.bcl_park_lamp_active_fault_code,       std::uint16_t{0xB004U});
    EXPECT_EQ(snap.bcl_head_lamp_active_fault_code,       std::uint16_t{0xB005U});
    EXPECT_EQ(snap.bcl_command_sequence_counter,          std::uint16_t{0xABCDU});
}

// ---------------------------------------------------------------------------
// Test 7: JSON shape
//
// Verify: 11 keys present, keys in alphabetical order, bool signals serialize
// as "true"/"false", uint16 signals serialize as decimal integers.
// ---------------------------------------------------------------------------

TEST(VssLampOverlayTest, JsonShape) {
    vss::VssSnapshot snap;
    snap.beam_low_is_on                          = true;
    snap.direction_indicator_left_is_signaling   = false;
    snap.direction_indicator_right_is_signaling  = true;
    snap.hazard_is_signaling                     = false;
    snap.parking_is_on                           = false;
    snap.bcl_command_sequence_counter            = 42U;
    snap.bcl_hazard_active_fault_code            = 0U;
    snap.bcl_head_lamp_active_fault_code         = 0U;
    // 0xB001 = 45057
    snap.bcl_left_indicator_active_fault_code    = 0xB001U;
    snap.bcl_park_lamp_active_fault_code         = 0U;
    // 0xB002 = 45058
    snap.bcl_right_indicator_active_fault_code   = 0xB002U;

    const std::string json = vss::VssLampOverlay::ToJson(snap);

    // All 11 expected keys must be present.
    const std::array<std::string, 11U> keys = {{
        "Vehicle.Body.Lights.Beam.Low.IsOn",
        "Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling",
        "Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling",
        "Vehicle.Body.Lights.Hazard.IsSignaling",
        "Vehicle.Body.Lights.Parking.IsOn",
        "Vehicle.Private.BCL.Lighting.CommandSequenceCounter",
        "Vehicle.Private.BCL.Lighting.Hazard.ActiveFaultCode",
        "Vehicle.Private.BCL.Lighting.HeadLamp.ActiveFaultCode",
        "Vehicle.Private.BCL.Lighting.LeftIndicator.ActiveFaultCode",
        "Vehicle.Private.BCL.Lighting.ParkLamp.ActiveFaultCode",
        "Vehicle.Private.BCL.Lighting.RightIndicator.ActiveFaultCode",
    }};

    for (const auto& key : keys) {
        EXPECT_NE(json.find(key), std::string::npos) << "Missing key: " << key;
    }

    // Keys must appear in alphabetical (sorted) order — positions strictly increasing.
    std::size_t prev_pos = 0U;
    for (const auto& key : keys) {
        const std::size_t pos = json.find(key);
        EXPECT_GT(pos, prev_pos) << "Key out of order: " << key;
        prev_pos = pos;
    }

    // Spot-check key-value pairs for type correctness.
    // beam_low_is_on = true → "...IsOn": true
    EXPECT_NE(json.find("\"Vehicle.Body.Lights.Beam.Low.IsOn\": true"), std::string::npos);
    // direction_indicator_left_is_signaling = false → "...IsSignaling": false
    EXPECT_NE(json.find(
        "\"Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling\": false"),
        std::string::npos);
    // CommandSequenceCounter = 42 (decimal)
    EXPECT_NE(json.find(
        "\"Vehicle.Private.BCL.Lighting.CommandSequenceCounter\": 42"),
        std::string::npos);
    // LeftIndicator fault = 45057 (0xB001)
    EXPECT_NE(json.find(
        "\"Vehicle.Private.BCL.Lighting.LeftIndicator.ActiveFaultCode\": 45057"),
        std::string::npos);
    // RightIndicator fault = 45058 (0xB002)
    EXPECT_NE(json.find(
        "\"Vehicle.Private.BCL.Lighting.RightIndicator.ActiveFaultCode\": 45058"),
        std::string::npos);
}

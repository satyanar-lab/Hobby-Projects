#include <gtest/gtest.h>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/application/uds_request_handler.hpp"
#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/uds_service_ids.hpp"

using body_control::lighting::application::RearLightingFunctionManager;
using body_control::lighting::application::UdsRequestHandler;
using body_control::lighting::domain::LampCommand;
using body_control::lighting::domain::LampCommandAction;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::uds::kSidNegativeResponse;
using body_control::lighting::domain::uds::kNrcServiceNotSupported;
using body_control::lighting::domain::uds::kNrcSubFunctionNotSupported;
using body_control::lighting::domain::uds::kNrcRequestOutOfRange;
using body_control::lighting::domain::uds::kPositiveResponseOffset;

// ── Fixture ───────────────────────────────────────────────────────────────────

class UdsHandlerTest : public ::testing::Test
{
protected:
    RearLightingFunctionManager fm {};
    UdsRequestHandler           handler {fm};

    // Helper: activate a lamp via the function manager.
    void ActivateLamp(const LampFunction fn)
    {
        LampCommand cmd {};
        cmd.function = fn;
        cmd.action   = LampCommandAction::kActivate;
        static_cast<void>(fm.ApplyCommand(cmd));
    }

    // Helper: inject a fault via the function manager.
    void InjectFault(const LampFunction fn)
    {
        static_cast<void>(fm.HandleFaultInjection(fn));
    }

    std::uint8_t ActiveFaultCount() const
    {
        return fm.GetFaultStatus().active_fault_count;
    }

    bool IsFaulted(const LampFunction fn) const
    {
        const auto status = fm.GetFaultStatus();
        const auto target = static_cast<std::uint16_t>(
            body_control::lighting::domain::LampFunctionToFaultCode(fn));
        for (std::size_t i = 0U; i < status.active_fault_count; ++i)
        {
            if (static_cast<std::uint16_t>(status.active_faults[i]) == target)
            {
                return true;
            }
        }
        return false;
    }

    // Helper: check that response is a negative response with expected NRC.
    static void ExpectNegativeResponse(
        const std::vector<std::uint8_t>& resp,
        const std::uint8_t               request_sid,
        const std::uint8_t               nrc)
    {
        ASSERT_EQ(resp.size(), 3U);
        EXPECT_EQ(resp[0], kSidNegativeResponse);
        EXPECT_EQ(resp[1], request_sid);
        EXPECT_EQ(resp[2], nrc);
    }
};

// ── 0x22 ReadDataByIdentifier — F101 lamp status ──────────────────────────────

TEST_F(UdsHandlerTest, ReadLampStatus_AllOff_ReturnsKOffForAllFunctions)
{
    const std::vector<std::uint8_t> req {0x22U, 0xF1U, 0x01U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_GE(resp.size(), 8U);
    EXPECT_EQ(resp[0], 0x62U);
    EXPECT_EQ(resp[1], 0xF1U);
    EXPECT_EQ(resp[2], 0x01U);

    // kOff = 1 for all five lamps when freshly constructed.
    for (std::size_t i = 3U; i < 8U; ++i)
    {
        EXPECT_EQ(resp[i], 0x01U) << "lamp index " << (i - 3U) << " should be kOff";
    }
}

TEST_F(UdsHandlerTest, ReadLampStatus_HeadLampOn_ReturnsKOnForHeadLamp)
{
    ActivateLamp(LampFunction::kHeadLamp);

    const std::vector<std::uint8_t> req {0x22U, 0xF1U, 0x01U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_GE(resp.size(), 8U);
    // HeadLamp is index 4 in response (byte offset 3+4=7).
    EXPECT_EQ(resp[7], 0x02U) << "head lamp should be kOn=2";
    // Others unchanged.
    for (std::size_t i = 3U; i < 7U; ++i)
    {
        EXPECT_EQ(resp[i], 0x01U) << "lamp index " << (i - 3U) << " should still be kOff";
    }
}

// ── 0x22 ReadDataByIdentifier — F103 active faults ───────────────────────────

TEST_F(UdsHandlerTest, ReadActiveFaults_NoFaults_CountIsZero)
{
    const std::vector<std::uint8_t> req {0x22U, 0xF1U, 0x03U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_GE(resp.size(), 4U);
    EXPECT_EQ(resp[0], 0x62U);
    EXPECT_EQ(resp[3], 0x00U) << "active_fault_count should be 0";
    EXPECT_EQ(resp.size(), 4U) << "no fault bytes should follow count";
}

TEST_F(UdsHandlerTest, ReadActiveFaults_AfterInject_FaultCodePresent)
{
    InjectFault(LampFunction::kLeftIndicator);

    const std::vector<std::uint8_t> req {0x22U, 0xF1U, 0x03U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_EQ(resp.size(), 6U);  // 3 header + 1 count + 2 code bytes
    EXPECT_EQ(resp[3], 0x01U) << "active_fault_count should be 1";
    // kLeftIndicator = 0xB001 → high byte 0xB0, low byte 0x01
    EXPECT_EQ(resp[4], 0xB0U);
    EXPECT_EQ(resp[5], 0x01U);
}

// ── 0x14 ClearDiagnosticInformation ──────────────────────────────────────────

TEST_F(UdsHandlerTest, ClearAllDtcs_ClearsFaultManager)
{
    InjectFault(LampFunction::kParkLamp);
    InjectFault(LampFunction::kHeadLamp);
    ASSERT_EQ(ActiveFaultCount(), 2U);

    const std::vector<std::uint8_t> req {0x14U, 0xFFU, 0xFFU, 0xFFU};
    const auto resp = handler.HandleRequest(req);

    ASSERT_EQ(resp.size(), 1U);
    EXPECT_EQ(resp[0], 0x54U) << "positive response 0x14+0x40=0x54";
    EXPECT_EQ(ActiveFaultCount(), 0U) << "faults should be cleared";
}

TEST_F(UdsHandlerTest, ClearDtcs_UnknownGroup_ReturnsNrc)
{
    const std::vector<std::uint8_t> req {0x14U, 0x00U, 0x00U, 0x01U};
    const auto resp = handler.HandleRequest(req);
    ExpectNegativeResponse(resp, 0x14U, kNrcRequestOutOfRange);
}

// ── 0x31 RoutineControl — inject / clear fault ────────────────────────────────

TEST_F(UdsHandlerTest, RoutineControl_InjectFault_FaultManagerUpdated)
{
    // Start (inject) for left indicator = routine ID 0xB001
    const std::vector<std::uint8_t> req {0x31U, 0x01U, 0xB0U, 0x01U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_EQ(resp.size(), 4U);
    EXPECT_EQ(resp[0], 0x71U) << "positive response 0x31+0x40=0x71";
    EXPECT_EQ(resp[1], 0x01U) << "sub-function echoed";
    EXPECT_EQ(resp[2], 0xB0U);
    EXPECT_EQ(resp[3], 0x01U);

    EXPECT_TRUE(IsFaulted(LampFunction::kLeftIndicator));
}

TEST_F(UdsHandlerTest, RoutineControl_ClearFault_FaultManagerUpdated)
{
    InjectFault(LampFunction::kRightIndicator);

    // Stop (clear) for right indicator = routine ID 0xB002
    const std::vector<std::uint8_t> req {0x31U, 0x02U, 0xB0U, 0x02U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_EQ(resp.size(), 4U);
    EXPECT_EQ(resp[0], 0x71U);
    EXPECT_FALSE(IsFaulted(LampFunction::kRightIndicator));
}

TEST_F(UdsHandlerTest, RoutineControl_UnknownRoutineId_ReturnsNrc)
{
    const std::vector<std::uint8_t> req {0x31U, 0x01U, 0xFFU, 0xFFU};
    const auto resp = handler.HandleRequest(req);
    ExpectNegativeResponse(resp, 0x31U, kNrcRequestOutOfRange);
}

// ── 0x7F Negative response for unknown SID ────────────────────────────────────

TEST_F(UdsHandlerTest, UnknownSid_ReturnsServiceNotSupported)
{
    const std::vector<std::uint8_t> req {0xABU};
    const auto resp = handler.HandleRequest(req);
    ExpectNegativeResponse(resp, 0xABU, kNrcServiceNotSupported);
}

TEST_F(UdsHandlerTest, EmptyRequest_ReturnsServiceNotSupported)
{
    const std::vector<std::uint8_t> req {};
    const auto resp = handler.HandleRequest(req);
    ASSERT_EQ(resp.size(), 3U);
    EXPECT_EQ(resp[0], kSidNegativeResponse);
}

// ── 0x10 DiagnosticSessionControl ────────────────────────────────────────────

TEST_F(UdsHandlerTest, DiagnosticSessionControl_DefaultSession_PositiveResponse)
{
    const std::vector<std::uint8_t> req {0x10U, 0x01U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_GE(resp.size(), 2U);
    EXPECT_EQ(resp[0], 0x50U) << "positive response 0x10+0x40=0x50";
    EXPECT_EQ(resp[1], 0x01U) << "session echoed";
}

TEST_F(UdsHandlerTest, DiagnosticSessionControl_ExtendedSession_PositiveResponse)
{
    const std::vector<std::uint8_t> req {0x10U, 0x03U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_GE(resp.size(), 2U);
    EXPECT_EQ(resp[0], 0x50U);
    EXPECT_EQ(resp[1], 0x03U);
}

TEST_F(UdsHandlerTest, DiagnosticSessionControl_UnknownSession_ReturnsNrc)
{
    const std::vector<std::uint8_t> req {0x10U, 0x05U};
    const auto resp = handler.HandleRequest(req);
    ExpectNegativeResponse(resp, 0x10U, kNrcSubFunctionNotSupported);
}

// ── 0x19 ReadDTCInformation ───────────────────────────────────────────────────

TEST_F(UdsHandlerTest, ReadDtcByStatusMask_NoFaults_EmptyDtcList)
{
    const std::vector<std::uint8_t> req {0x19U, 0x02U, 0x09U};
    const auto resp = handler.HandleRequest(req);

    ASSERT_EQ(resp.size(), 3U);
    EXPECT_EQ(resp[0], 0x59U) << "positive response 0x19+0x40=0x59";
    EXPECT_EQ(resp[1], 0x02U);
}

TEST_F(UdsHandlerTest, ReadDtcByStatusMask_AfterInject_ReturnsDtcRecord)
{
    InjectFault(LampFunction::kHazardLamp);

    const std::vector<std::uint8_t> req {0x19U, 0x02U, 0x09U};
    const auto resp = handler.HandleRequest(req);

    // 3 header + 4 bytes per DTC record (3-byte DTC + 1-byte status)
    ASSERT_EQ(resp.size(), 7U);
    // kHazardLamp = 0xB003 → {0x00, 0xB0, 0x03}
    EXPECT_EQ(resp[3], 0x00U);
    EXPECT_EQ(resp[4], 0xB0U);
    EXPECT_EQ(resp[5], 0x03U);
    EXPECT_EQ(resp[6], 0x09U) << "status byte: testFailed + confirmedDTC";
}

TEST_F(UdsHandlerTest, ReadDtcSupported_ReturnsFiveEntries)
{
    const std::vector<std::uint8_t> req {0x19U, 0x0AU};
    const auto resp = handler.HandleRequest(req);

    // 3 header + 5 × 4 bytes = 23 bytes
    ASSERT_EQ(resp.size(), 23U);
    EXPECT_EQ(resp[0], 0x59U);
    EXPECT_EQ(resp[1], 0x0AU);
}

#include <gtest/gtest.h>

#include "body_control/lighting/application/fault_manager.hpp"
#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

using body_control::lighting::application::FaultManager;
using body_control::lighting::application::FaultManagerStatus;
using body_control::lighting::domain::FaultCode;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampFaultStatus;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::domain::NodeHealthState;

// Convenience helpers so setup lines don't need [[nodiscard]] casts everywhere.
static void Inject(FaultManager& fm, LampFunction f)
{
    static_cast<void>(fm.InjectFault(f));
}
static void Clear(FaultManager& fm, LampFunction f)
{
    static_cast<void>(fm.ClearFault(f));
}

// ── InjectFault ──────────────────────────────────────────────────────────────

TEST(FaultManagerTest, InjectFault_NewFault_ReturnsSuccess)
{
    FaultManager fm;
    EXPECT_EQ(fm.InjectFault(LampFunction::kLeftIndicator),
              FaultManagerStatus::kSuccess);
}

TEST(FaultManagerTest, InjectFault_BlocksActivation_IsFaultedTrue)
{
    FaultManager fm;
    Inject(fm, LampFunction::kLeftIndicator);
    EXPECT_TRUE(fm.IsFaulted(LampFunction::kLeftIndicator));
}

TEST(FaultManagerTest, InjectFault_OtherFunctions_NotFaulted)
{
    FaultManager fm;
    Inject(fm, LampFunction::kLeftIndicator);
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kRightIndicator));
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kHazardLamp));
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kParkLamp));
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kHeadLamp));
}

TEST(FaultManagerTest, InjectFault_AlreadyFaulted_ReturnsAlreadyFaulted)
{
    FaultManager fm;
    Inject(fm, LampFunction::kLeftIndicator);
    EXPECT_EQ(fm.InjectFault(LampFunction::kLeftIndicator),
              FaultManagerStatus::kAlreadyFaulted);
}

TEST(FaultManagerTest, InjectFault_UnknownFunction_ReturnsInvalidFunction)
{
    FaultManager fm;
    EXPECT_EQ(fm.InjectFault(LampFunction::kUnknown),
              FaultManagerStatus::kInvalidFunction);
}

// ── ClearFault ───────────────────────────────────────────────────────────────

TEST(FaultManagerTest, ClearFault_ExistingFault_ReturnsSuccess)
{
    FaultManager fm;
    Inject(fm, LampFunction::kRightIndicator);
    EXPECT_EQ(fm.ClearFault(LampFunction::kRightIndicator),
              FaultManagerStatus::kSuccess);
}

TEST(FaultManagerTest, ClearFault_RestoresActivation)
{
    FaultManager fm;
    Inject(fm, LampFunction::kRightIndicator);
    Clear(fm, LampFunction::kRightIndicator);
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kRightIndicator));
}

TEST(FaultManagerTest, ClearFault_NotFaulted_ReturnsNotFaulted)
{
    FaultManager fm;
    EXPECT_EQ(fm.ClearFault(LampFunction::kRightIndicator),
              FaultManagerStatus::kNotFaulted);
}

TEST(FaultManagerTest, ClearFault_UnknownFunction_ReturnsInvalidFunction)
{
    FaultManager fm;
    EXPECT_EQ(fm.ClearFault(LampFunction::kUnknown),
              FaultManagerStatus::kInvalidFunction);
}

// ── Multiple faults tracked independently ────────────────────────────────────

TEST(FaultManagerTest, MultipleFaults_TrackedIndependently)
{
    FaultManager fm;
    Inject(fm, LampFunction::kLeftIndicator);
    Inject(fm, LampFunction::kParkLamp);
    Inject(fm, LampFunction::kHeadLamp);

    EXPECT_TRUE(fm.IsFaulted(LampFunction::kLeftIndicator));
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kRightIndicator));
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kHazardLamp));
    EXPECT_TRUE(fm.IsFaulted(LampFunction::kParkLamp));
    EXPECT_TRUE(fm.IsFaulted(LampFunction::kHeadLamp));
    EXPECT_EQ(fm.ActiveFaultCount(), 3U);
}

TEST(FaultManagerTest, ClearOneOfMultipleFaults_OthersUnaffected)
{
    FaultManager fm;
    Inject(fm, LampFunction::kLeftIndicator);
    Inject(fm, LampFunction::kParkLamp);
    Clear(fm, LampFunction::kLeftIndicator);

    EXPECT_FALSE(fm.IsFaulted(LampFunction::kLeftIndicator));
    EXPECT_TRUE(fm.IsFaulted(LampFunction::kParkLamp));
    EXPECT_EQ(fm.ActiveFaultCount(), 1U);
}

// ── GetFaultStatus ────────────────────────────────────────────────────────────

TEST(FaultManagerTest, GetFaultStatus_NoFaults_AllClear)
{
    FaultManager fm;
    const LampFaultStatus status = fm.GetFaultStatus();

    EXPECT_FALSE(status.fault_present);
    EXPECT_EQ(status.active_fault_count, 0U);
    for (const auto& code : status.active_faults)
    {
        EXPECT_EQ(code, FaultCode::kNoFault);
    }
}

TEST(FaultManagerTest, GetFaultStatus_OneFault_CorrectCode)
{
    FaultManager fm;
    Inject(fm, LampFunction::kHazardLamp);
    const LampFaultStatus status = fm.GetFaultStatus();

    EXPECT_TRUE(status.fault_present);
    EXPECT_EQ(status.active_fault_count, 1U);
    EXPECT_EQ(status.active_faults[0], FaultCode::kHazardLamp);
}

TEST(FaultManagerTest, GetFaultStatus_AfterClear_FaultPresentFalse)
{
    FaultManager fm;
    Inject(fm, LampFunction::kParkLamp);
    Clear(fm, LampFunction::kParkLamp);
    const LampFaultStatus status = fm.GetFaultStatus();

    EXPECT_FALSE(status.fault_present);
    EXPECT_EQ(status.active_fault_count, 0U);
}

// ── IsFaulted after inject/clear/inject sequence ─────────────────────────────

TEST(FaultManagerTest, InjectClearInject_CorrectState)
{
    FaultManager fm;

    Inject(fm, LampFunction::kHeadLamp);
    EXPECT_TRUE(fm.IsFaulted(LampFunction::kHeadLamp));

    Clear(fm, LampFunction::kHeadLamp);
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kHeadLamp));

    Inject(fm, LampFunction::kHeadLamp);
    EXPECT_TRUE(fm.IsFaulted(LampFunction::kHeadLamp));
    EXPECT_EQ(fm.ActiveFaultCount(), 1U);
}

// ── ClearAllFaults ────────────────────────────────────────────────────────────

TEST(FaultManagerTest, ClearAllFaults_ResetsEverything)
{
    FaultManager fm;
    Inject(fm, LampFunction::kLeftIndicator);
    Inject(fm, LampFunction::kRightIndicator);
    Inject(fm, LampFunction::kHazardLamp);

    fm.ClearAllFaults();

    EXPECT_EQ(fm.ActiveFaultCount(), 0U);
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kLeftIndicator));
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kRightIndicator));
    EXPECT_FALSE(fm.IsFaulted(LampFunction::kHazardLamp));
}

// ── PopulateHealth ────────────────────────────────────────────────────────────

TEST(FaultManagerTest, PopulateHealth_WithFault_SetsHealthFaulted)
{
    FaultManager fm;
    Inject(fm, LampFunction::kLeftIndicator);

    NodeHealthStatus health {};
    health.health_state            = NodeHealthState::kOperational;
    health.ethernet_link_available = true;
    health.service_available       = true;

    fm.PopulateHealth(health);

    EXPECT_TRUE(health.lamp_driver_fault_present);
    EXPECT_EQ(health.active_fault_count, 1U);
    EXPECT_EQ(health.health_state, NodeHealthState::kFaulted);
}

TEST(FaultManagerTest, PopulateHealth_NoFault_HealthUnchanged)
{
    FaultManager fm;

    NodeHealthStatus health {};
    health.health_state              = NodeHealthState::kOperational;
    health.lamp_driver_fault_present = false;
    health.active_fault_count        = 0U;

    fm.PopulateHealth(health);

    EXPECT_FALSE(health.lamp_driver_fault_present);
    EXPECT_EQ(health.active_fault_count, 0U);
    EXPECT_EQ(health.health_state, NodeHealthState::kOperational);
}

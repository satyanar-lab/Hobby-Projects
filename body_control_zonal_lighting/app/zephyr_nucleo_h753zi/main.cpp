// Zephyr RTOS rear lighting node — NUCLEO-H753ZI
//
// Phase 9: Zephyr RTOS port.  Replaces the Phase 6/7 bare-metal superloop
// with four RTOS threads connected by a Zephyr message queue:
//
//   udp_rx_thread  — blocks on UDP recv, posts LampCommand to g_lamp_cmd_queue
//   cmd_thread     — dequeues commands, applies arbitration, sends LampStatus
//   blink_thread   — 20 ms periodic tick, drives GPIO for blink patterns
//   health_thread  — 1000 ms periodic, publishes NodeHealthStatusEvent
//
// Network configuration (static, same as bare-metal build):
//   NUCLEO IP  : 192.168.0.20  (kNucleoIpStr / prj.conf NET_CONFIG_MY_IPV4_ADDR)
//   CZC IP     : 192.168.0.10  (kCzcIpStr)
//   Local port : 41001         (kRearLightingNodePort)
//   Remote port: 41000         (kCentralZoneControllerPort)

#include <array>
#include <cstdint>
#include <cstring>

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "body_control/lighting/application/command_arbitrator.hpp"
#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_constants.hpp"
#include "body_control/lighting/domain/lighting_payload_codec.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"
#include "zephyr_gpio_driver.hpp"
#include "zephyr_udp_transport.hpp"

LOG_MODULE_REGISTER(rear_lighting_node, LOG_LEVEL_INF);

// ---- Type aliases ----------------------------------------------------------

using LF  = body_control::lighting::domain::LampFunction;
using LCA = body_control::lighting::domain::LampCommandAction;
using LOS = body_control::lighting::domain::LampOutputState;
using LC  = body_control::lighting::domain::LampCommand;
using LS  = body_control::lighting::domain::LampStatus;
using NHS = body_control::lighting::domain::NodeHealthStatus;
using NHState = body_control::lighting::domain::NodeHealthState;

namespace bld_transport = body_control::lighting::transport;
namespace zudp = body_control::lighting::transport::zephyr_transport;
namespace zgpio = body_control::lighting::platform::zephyr_platform;

// ---- Network constants -----------------------------------------------------

static constexpr const char*   kCzcIpStr                 {"192.168.0.10"};
static constexpr std::uint16_t kRearLightingNodePort      {41001U};
static constexpr std::uint16_t kCentralZoneControllerPort {41000U};

// ---- Shared resources ------------------------------------------------------
//
// All file-scope; K_* macros must be at file scope (not inside a namespace)
// because they use STRUCT_SECTION_ITERABLE and K_THREAD_STACK_DEFINE place
// objects in dedicated linker sections that the Zephyr linker script expects
// at file scope.

// k_mutex carries priority inheritance — if blink_thread (higher priority)
// blocks on a mutex held by cmd_thread (lower), Zephyr temporarily boosts
// cmd_thread to prevent priority inversion.
static K_MUTEX_DEFINE(g_lamp_mgr_mutex);

static body_control::lighting::application::RearLightingFunctionManager
    g_lamp_mgr;

// Queue depth of 8: the CZC sends at most one command per operator action;
// burst-clicking the HMI faster than cmd_thread can drain is bounded by
// human reaction time.  8 slots give ample headroom.
K_MSGQ_DEFINE(g_lamp_cmd_queue, sizeof(LC), 8, 4);

static zgpio::ZephyrGpioDriver g_gpio;

static const zudp::ZephyrUdpConfig kUdpConfig {
    kCzcIpStr,
    kRearLightingNodePort,
    kCentralZoneControllerPort
};
static zudp::ZephyrUdpTransportAdapter g_transport {kUdpConfig};

// ---- Thread stacks ---------------------------------------------------------

K_THREAD_STACK_DEFINE(g_udp_rx_stack,  1536);
K_THREAD_STACK_DEFINE(g_cmd_stack,     2048);
K_THREAD_STACK_DEFINE(g_blink_stack,   1024);
K_THREAD_STACK_DEFINE(g_health_stack,  1536);

static struct k_thread g_udp_rx_thread_data;
static struct k_thread g_cmd_thread_data;
static struct k_thread g_blink_thread_data;
static struct k_thread g_health_thread_data;

// ---- Blink manager ---------------------------------------------------------
//
// Identical phase-timer logic as the bare-metal BlinkManager in
// app/stm32_nucleo_h753zi/main.cpp, adapted to ZephyrGpioDriver.
// Called exclusively from blink_thread; no mutex needed here because only
// blink_thread reads lamp state (under g_lamp_mgr_mutex) and writes GPIO.

namespace
{

class BlinkManager
{
public:
    explicit BlinkManager(
        zgpio::ZephyrGpioDriver&                                        gpio,
        body_control::lighting::application::RearLightingFunctionManager& fmgr)
        noexcept
        : gpio_(gpio), fmgr_(fmgr)
    {}

    void ProcessTick(const std::uint32_t now_ms) noexcept
    {
        const bool hazard_on = IsOn(LF::kHazardLamp);

        if (hazard_on)
        {
            if (!hazard_blink_.was_active)
            {
                hazard_blink_ = BlinkState {true, now_ms, true};
            }
            else
            {
                Tick(now_ms, hazard_blink_);
            }
            const bool phase = hazard_blink_.phase_on;
            static_cast<void>(gpio_.WriteLampOutput(LF::kHazardLamp,     phase));
            static_cast<void>(gpio_.WriteLampOutput(LF::kLeftIndicator,  phase));
            static_cast<void>(gpio_.WriteLampOutput(LF::kRightIndicator, phase));
        }
        else
        {
            hazard_blink_ = BlinkState {};
            static_cast<void>(gpio_.WriteLampOutput(LF::kHazardLamp, false));
            DriveIndicator(now_ms, LF::kLeftIndicator,  left_blink_);
            DriveIndicator(now_ms, LF::kRightIndicator, right_blink_);
        }

        static_cast<void>(gpio_.WriteLampOutput(
            LF::kParkLamp, IsOn(LF::kParkLamp)));
        static_cast<void>(gpio_.WriteLampOutput(
            LF::kHeadLamp, IsOn(LF::kHeadLamp)));
    }

private:
    struct BlinkState
    {
        bool          phase_on       {false};
        std::uint32_t phase_start_ms {0U};
        bool          was_active     {false};
    };

    bool IsOn(const LF func) const noexcept
    {
        LS status {};
        fmgr_.GetLampStatus(func, status);
        return status.output_state == LOS::kOn;
    }

    void Tick(const std::uint32_t now_ms, BlinkState& state) const noexcept
    {
        using body_control::lighting::domain::timing::kIndicatorOnTime;
        using body_control::lighting::domain::timing::kIndicatorOffTime;

        const std::uint32_t period_ms =
            state.phase_on
                ? static_cast<std::uint32_t>(kIndicatorOnTime.count())
                : static_cast<std::uint32_t>(kIndicatorOffTime.count());

        if ((now_ms - state.phase_start_ms) >= period_ms)
        {
            state.phase_on       = !state.phase_on;
            state.phase_start_ms = now_ms;
        }
    }

    void DriveIndicator(
        const std::uint32_t now_ms,
        const LF            func,
        BlinkState&         state) noexcept
    {
        if (IsOn(func))
        {
            if (!state.was_active) { state = BlinkState {true, now_ms, true}; }
            else                   { Tick(now_ms, state); }
            static_cast<void>(gpio_.WriteLampOutput(func, state.phase_on));
        }
        else
        {
            state = BlinkState {};
            static_cast<void>(gpio_.WriteLampOutput(func, false));
        }
    }

    zgpio::ZephyrGpioDriver&                                          gpio_;
    body_control::lighting::application::RearLightingFunctionManager& fmgr_;
    BlinkState left_blink_   {};
    BlinkState right_blink_  {};
    BlinkState hazard_blink_ {};
};

// ---- Arbitration helpers ---------------------------------------------------
//
// Ported from RearLightingNodeHandler in the bare-metal main.cpp.
// Runs in cmd_thread (single consumer of g_lamp_cmd_queue).

// last_hazard_sequence_: sequence counter of the most recent hazard command.
// Indicator commands sharing this counter are companion fanout commands from
// the CZC and must be suppressed — otherwise they activate indicators
// immediately after a hazard-off toggle.
static std::uint16_t g_last_hazard_sequence {0U};

static body_control::lighting::application::IndicatorInputRegistry g_indicator_registry {};

void SendLampStatusEvent(const LF func) noexcept
{
    LS status {};
    if (g_lamp_mgr.GetLampStatus(func, status))
    {
        const auto msg =
            bld_transport::SomeipMessageBuilder::BuildLampStatusEvent(status);
        static_cast<void>(g_transport.SendEvent(msg));
    }
}

bool IsOn(const LF func) noexcept
{
    LS status {};
    g_lamp_mgr.GetLampStatus(func, status);
    return status.output_state == LOS::kOn;
}

void HandleHazard(const LC& cmd) noexcept
{
    g_last_hazard_sequence = cmd.sequence_counter;

    const bool hazard_was_on = IsOn(LF::kHazardLamp);

    LC resolved     = cmd;
    resolved.action = hazard_was_on ? LCA::kDeactivate : LCA::kActivate;
    static_cast<void>(g_lamp_mgr.ApplyCommand(resolved));

    if (hazard_was_on)
    {
        g_indicator_registry.hazard_active = false;

        // Deactivate both indicators, then restore the pre-hazard one.
        LC off      = cmd;
        off.action  = LCA::kDeactivate;
        off.function = LF::kLeftIndicator;
        static_cast<void>(g_lamp_mgr.ApplyCommand(off));
        off.function = LF::kRightIndicator;
        static_cast<void>(g_lamp_mgr.ApplyCommand(off));
        SendLampStatusEvent(LF::kHazardLamp);
        SendLampStatusEvent(LF::kLeftIndicator);
        SendLampStatusEvent(LF::kRightIndicator);

        if (g_indicator_registry.active_indicator != LF::kUnknown)
        {
            LC restore       = cmd;
            restore.function = g_indicator_registry.active_indicator;
            restore.action   = LCA::kActivate;
            static_cast<void>(g_lamp_mgr.ApplyCommand(restore));
            SendLampStatusEvent(g_indicator_registry.active_indicator);
            g_indicator_registry.active_indicator = LF::kUnknown;
        }

        LOG_INF("Hazard: OFF");
    }
    else
    {
        // Capture which indicator was active before hazard overrides the output.
        if      (IsOn(LF::kLeftIndicator))  { g_indicator_registry.active_indicator = LF::kLeftIndicator;  }
        else if (IsOn(LF::kRightIndicator)) { g_indicator_registry.active_indicator = LF::kRightIndicator; }
        else                                { g_indicator_registry.active_indicator = LF::kUnknown;         }
        g_indicator_registry.hazard_active = true;

        SendLampStatusEvent(LF::kHazardLamp);
        LOG_INF("Hazard: ON");
    }
}

void HandleIndicator(const LC& cmd) noexcept
{
    // Suppress companion indicator commands sent as part of a hazard fanout.
    if (cmd.sequence_counter == g_last_hazard_sequence) { return; }

    if (IsOn(LF::kHazardLamp))
    {
        LOG_WRN("Indicator blocked: hazard active");
        return;
    }

    LC resolved = cmd;
    if (cmd.action == LCA::kToggle)
    {
        resolved.action = IsOn(cmd.function) ? LCA::kDeactivate : LCA::kActivate;
    }

    if (resolved.action == LCA::kActivate)
    {
        // Record operator intent so hazard-off can restore this indicator.
        g_indicator_registry.active_indicator = cmd.function;

        // Exclusivity: activating one indicator deactivates the opposite.
        const LF opposite = (cmd.function == LF::kLeftIndicator)
                                ? LF::kRightIndicator
                                : LF::kLeftIndicator;
        if (IsOn(opposite))
        {
            LC deactivate       = resolved;
            deactivate.function = opposite;
            deactivate.action   = LCA::kDeactivate;
            static_cast<void>(g_lamp_mgr.ApplyCommand(deactivate));
            SendLampStatusEvent(opposite);
        }
    }
    else if (resolved.action == LCA::kDeactivate)
    {
        // Stalk to centre: clear the registry so hazard-off won't restore.
        if (g_indicator_registry.active_indicator == cmd.function)
        {
            g_indicator_registry.active_indicator = LF::kUnknown;
        }
    }

    static_cast<void>(g_lamp_mgr.ApplyCommand(resolved));
    SendLampStatusEvent(cmd.function);

    LS result {};
    if (g_lamp_mgr.GetLampStatus(cmd.function, result))
    {
        LOG_INF("Lamp command applied: %s",
                (result.output_state == LOS::kOn) ? "ON" : "OFF");
    }
}

void ApplyLampCommand(const LC& cmd) noexcept
{
    k_mutex_lock(&g_lamp_mgr_mutex, K_FOREVER);

    if (cmd.function == LF::kHazardLamp)
    {
        HandleHazard(cmd);
    }
    else if ((cmd.function == LF::kLeftIndicator)
          || (cmd.function == LF::kRightIndicator))
    {
        HandleIndicator(cmd);
    }
    else
    {
        // Park and head lamps: no arbitration.
        if (g_lamp_mgr.ApplyCommand(cmd))
        {
            SendLampStatusEvent(cmd.function);
            LS result {};
            if (g_lamp_mgr.GetLampStatus(cmd.function, result))
            {
                LOG_INF("Lamp command applied: %s",
                        (result.output_state == LOS::kOn) ? "ON" : "OFF");
            }
        }
    }

    k_mutex_unlock(&g_lamp_mgr_mutex);
}

}  // namespace

// ---- Thread functions ------------------------------------------------------
//
// Signature matches k_thread_entry_t: void fn(void*, void*, void*).
// Unused parameters are explicitly cast to void to satisfy -Wunused-parameter.

static void UdpRxThread(void* /*p1*/, void* /*p2*/, void* /*p3*/)
{
    LOG_INF("UDP rx thread started");

    while (true)
    {
        // ReceiveBlocking() blocks until a datagram arrives, then decodes it
        // and calls OnTransportMessageReceived on the registered handler.
        // The handler (LampCommandDispatcher below) posts to the queue.
        g_transport.ReceiveBlocking();
    }
}

// Handler registered with the transport; called from udp_rx_thread's context.
// Validates the message type, parses the LampCommand, and posts to the queue.
// Defined as a file-scope struct to avoid capturing a lambda (which cannot
// convert to k_thread_entry_t) and to keep the handler lifetime static.
namespace
{

struct LampCommandDispatcher final
    : public bld_transport::TransportMessageHandlerInterface
{
    void OnTransportMessageReceived(
        const bld_transport::TransportMessage& msg) override
    {
        LOG_WRN("RX msg: svc=0x%04x method=0x%04x is_event=%d payload=%u",
                msg.service_id, msg.method_or_event_id,
                static_cast<int>(msg.is_event),
                static_cast<unsigned>(msg.payload.size()));

        if (!bld_transport::SomeipMessageParser::IsSetLampCommandRequest(msg))
        {
            LOG_WRN("RX: not SetLampCommandRequest, ignored");
            return;
        }

        // Guard payload size before calling ParseLampCommand.
        // Wire format: func(1) + action(1) + source(1) + sequence_counter(2) = 5 bytes.
        // kLampCommandPayloadLength (8) is the codec buffer size with reserved padding
        // bytes — a different (unused) encoding.  ParseLampCommand reads offsets 0-4,
        // so a payload shorter than 5 bytes would cause an out-of-bounds read.
        constexpr std::size_t kLampCommandWireLength {5U};
        if (msg.payload.size() != kLampCommandWireLength)
        {
            LOG_WRN("RX: bad payload size %u (want %u)",
                    static_cast<unsigned>(msg.payload.size()),
                    static_cast<unsigned>(kLampCommandWireLength));
            return;
        }

        const LC cmd =
            bld_transport::SomeipMessageParser::ParseLampCommand(msg);

        LOG_WRN("RX: SetLampCommand OK func=%d action=%d seq=%u",
                static_cast<int>(cmd.function),
                static_cast<int>(cmd.action),
                static_cast<unsigned>(cmd.sequence_counter));

        // Queue depth 8 is sufficient for any realistic burst (max 3 commands
        // per operator action from the CZC fanout).  Purging is intentionally
        // avoided: a hazard activate is always followed immediately by two
        // companion indicator commands with the same sequence counter, and
        // purging would drop the hazard command before cmd_thread can process it.
        const int ret = k_msgq_put(&g_lamp_cmd_queue, &cmd, K_NO_WAIT);
        if (ret != 0)
        {
            LOG_WRN("lamp cmd queue full, datagram dropped");
        }
    }

    void OnTransportAvailabilityChanged(const bool is_available) override
    {
        if (is_available) { LOG_INF("Transport up");   }
        else              { LOG_WRN("Transport down"); }
    }
};

static LampCommandDispatcher g_dispatcher;

}  // namespace

static void CmdThread(void* /*p1*/, void* /*p2*/, void* /*p3*/)
{
    LOG_INF("Command thread started");

    LC cmd {};
    while (true)
    {
        k_msgq_get(&g_lamp_cmd_queue, &cmd, K_FOREVER);
        ApplyLampCommand(cmd);
    }
}

static void BlinkThread(void* /*p1*/, void* /*p2*/, void* /*p3*/)
{
    LOG_INF("Blink thread started (20 ms period)");

    BlinkManager blink_mgr {g_gpio, g_lamp_mgr};

    while (true)
    {
        k_sleep(K_MSEC(20));

        const std::uint32_t now_ms = k_uptime_get_32();

        // Take the mutex for all five GetLampStatus calls as a single
        // critical section.  The window is ~5 register reads on H7 (~40 ns)
        // so contention with cmd_thread is negligible.
        k_mutex_lock(&g_lamp_mgr_mutex, K_FOREVER);
        blink_mgr.ProcessTick(now_ms);
        k_mutex_unlock(&g_lamp_mgr_mutex);
    }
}

static void HealthThread(void* /*p1*/, void* /*p2*/, void* /*p3*/)
{
    LOG_INF("Health publisher thread started (200 ms lamp broadcast, 1000 ms health)");

    static constexpr std::array<LF, 5U> kAllFunctions {
        LF::kLeftIndicator, LF::kRightIndicator, LF::kHazardLamp,
        LF::kParkLamp, LF::kHeadLamp
    };

    std::uint32_t tick_count {0U};

    while (true)
    {
        k_sleep(K_MSEC(200));
        ++tick_count;

        // Broadcast all lamp states every 200 ms so the HMI can self-correct drift.
        k_mutex_lock(&g_lamp_mgr_mutex, K_FOREVER);
        for (const LF func : kAllFunctions)
        {
            SendLampStatusEvent(func);
        }
        k_mutex_unlock(&g_lamp_mgr_mutex);

        // Publish node health every second (every 5th 200 ms tick).
        if ((tick_count % 5U) == 0U)
        {
            NHS health {};
            health.health_state              = NHState::kOperational;
            health.ethernet_link_available   = true;
            health.service_available         = true;
            health.lamp_driver_fault_present = false;
            health.active_fault_count        = 0U;

            const auto msg =
                bld_transport::SomeipMessageBuilder::BuildNodeHealthEvent(health);
            const auto tx_status = g_transport.SendEvent(msg);
            if (tx_status != bld_transport::TransportStatus::kSuccess)
            {
                LOG_WRN("Health TX failed: %d", static_cast<int>(tx_status));
            }
            else
            {
                LOG_DBG("Health TX OK");
            }
        }
    }
}

// ---- Entry point -----------------------------------------------------------

int main()
{
    LOG_INF("Rear lighting node starting (Zephyr RTOS)");

    // GPIO driver
    if (g_gpio.Initialize() != zgpio::GpioDriverStatus::kSuccess)
    {
        LOG_ERR("GPIO init failed");
        return -1;
    }
    LOG_INF("GPIO init OK");

    // UDP transport
    g_transport.SetMessageHandler(&g_dispatcher);
    if (g_transport.Initialize() != bld_transport::TransportStatus::kSuccess)
    {
        LOG_ERR("Transport init failed");
        return -1;
    }
    LOG_INF("Transport init OK — listening on :%u", kRearLightingNodePort);

    // Spawn threads — lower priority number = higher Zephyr preemptive priority.
    k_thread_create(&g_blink_thread_data,
                    g_blink_stack,
                    K_THREAD_STACK_SIZEOF(g_blink_stack),
                    BlinkThread,
                    nullptr, nullptr, nullptr,
                    3, 0, K_NO_WAIT);
    k_thread_name_set(&g_blink_thread_data, "blink");

    k_thread_create(&g_udp_rx_thread_data,
                    g_udp_rx_stack,
                    K_THREAD_STACK_SIZEOF(g_udp_rx_stack),
                    UdpRxThread,
                    nullptr, nullptr, nullptr,
                    4, 0, K_NO_WAIT);
    k_thread_name_set(&g_udp_rx_thread_data, "udp_rx");

    k_thread_create(&g_health_thread_data,
                    g_health_stack,
                    K_THREAD_STACK_SIZEOF(g_health_stack),
                    HealthThread,
                    nullptr, nullptr, nullptr,
                    5, 0, K_NO_WAIT);
    k_thread_name_set(&g_health_thread_data, "health");

    k_thread_create(&g_cmd_thread_data,
                    g_cmd_stack,
                    K_THREAD_STACK_SIZEOF(g_cmd_stack),
                    CmdThread,
                    nullptr, nullptr, nullptr,
                    6, 0, K_NO_WAIT);
    k_thread_name_set(&g_cmd_thread_data, "cmd");

    LOG_INF("Rear lighting node started");

    // main() returns; the Zephyr idle thread takes over.  All work is in the
    // four spawned threads which run indefinitely.
    return 0;
}

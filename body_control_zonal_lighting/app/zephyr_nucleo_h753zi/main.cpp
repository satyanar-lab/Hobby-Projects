// Zephyr RTOS exterior lighting node — NUCLEO-H753ZI
//
// Phase 9:  Zephyr RTOS port.  Four threads over a Zephyr message queue:
//   udp_rx_thread  — blocks on UDP recv, posts LampCommand to g_lamp_cmd_queue
//   cmd_thread     — dequeues commands, applies arbitration, sends LampStatus
//   blink_thread   — 20 ms periodic tick, drives GPIO for blink patterns
//   health_thread  — 200 ms broadcast + 1000 ms NodeHealthStatus publish
//
// Phase 13: DoIP/UDS TCP thread for OTA firmware update:
//   doip_thread    — blocks on accept(), handles DoIP framing (ISO 13400-2),
//                    dispatches UDS requests to UdsRequestHandler.
//                    UDS 0x34/0x36/0x37 write the incoming image to
//                    slot1_partition via stream_flash, then call
//                    boot_request_upgrade(BOOT_UPGRADE_TEST) and reboot.
//
//   DoIP uses Zephyr BSD sockets (not LwIP raw API) because the Zephyr
//   networking stack does not expose the LwIP raw-API callback model used
//   by the bare-metal STM32 DoipTcpServer.  The protocol framing logic is
//   identical; only the socket layer differs.  See app/stm32_nucleo_h753zi/
//   main.cpp for the LwIP version.
//
// Network configuration (static):
//   NUCLEO IP  : 192.168.0.20  (prj.conf NET_CONFIG_MY_IPV4_ADDR)
//   CZC IP     : 192.168.0.10  (kCzcIpStr)
//   Local port : 41001         (kExteriorLightingNodePort)
//   Remote port: 41000         (kCentralZoneControllerPort)
//   DoIP port  : 13400         (ISO 13400-2, kDoipPort)

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

#include <zephyr/dfu/mcuboot.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/socket.h>

#include "body_control/lighting/application/command_arbitrator.hpp"
#include "body_control/lighting/application/exterior_lighting_function_manager.hpp"
#include "body_control/lighting/application/uds_request_handler.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_constants.hpp"
#include "body_control/lighting/domain/lighting_payload_codec.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/transport/some_ip_sd_codec.hpp"
#include "body_control/lighting/transport/some_ip_sd_types.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"
#include "zephyr_gpio_driver.hpp"
#include "zephyr_udp_transport.hpp"

LOG_MODULE_REGISTER(exterior_lighting_node, LOG_LEVEL_INF);

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
static constexpr std::uint16_t kExteriorLightingNodePort      {41001U};
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

static body_control::lighting::application::ExteriorLightingFunctionManager
    g_lamp_mgr;

// Queue depth of 8: the CZC sends at most one command per operator action;
// burst-clicking the HMI faster than cmd_thread can drain is bounded by
// human reaction time.  8 slots give ample headroom.
K_MSGQ_DEFINE(g_lamp_cmd_queue, sizeof(LC), 8, 4);

static zgpio::ZephyrGpioDriver g_gpio;

static const zudp::ZephyrUdpConfig kUdpConfig {
    kCzcIpStr,
    kExteriorLightingNodePort,
    kCentralZoneControllerPort
};
static zudp::ZephyrUdpTransportAdapter g_transport {kUdpConfig};

// ---- Thread stacks ---------------------------------------------------------

K_THREAD_STACK_DEFINE(g_udp_rx_stack,  1536);
K_THREAD_STACK_DEFINE(g_cmd_stack,     2048);
K_THREAD_STACK_DEFINE(g_blink_stack,   1024);
K_THREAD_STACK_DEFINE(g_health_stack,  1536);
// DoIP thread uses BSD sockets with a 1024-byte static recv buffer on its stack.
// 3072 bytes covers the socket frame + UdsRequestHandler std::vector allocations.
K_THREAD_STACK_DEFINE(g_doip_stack,    3072);

static struct k_thread g_udp_rx_thread_data;
static struct k_thread g_cmd_thread_data;
static struct k_thread g_blink_thread_data;
static struct k_thread g_health_thread_data;
static struct k_thread g_doip_thread_data;

// UDS request handler — accessed only from DoIP thread; no locking needed.
static body_control::lighting::application::UdsRequestHandler
    g_uds_handler {g_lamp_mgr};

// ---- SOME/IP-SD OfferService (Phase 15) ------------------------------------
//
// Periodic SOME/IP-SD OfferService sent to multicast 224.244.224.245:30490
// every 2 seconds (main-phase only; initial-wait and repetition phases
// are intentionally omitted per AUTOSAR AP_PRS_SOMEIPServiceDiscovery §4.x).
// Wireshark's SOME/IP-SD dissector recognises each frame as a valid
// OfferService for ExteriorLightingService (0x5100).

static int              g_sd_fd      {-1};
static std::uint16_t    g_sd_session {1U};

static void SdOfferWorkHandler(struct k_work* work) noexcept;
static K_WORK_DELAYABLE_DEFINE(g_sd_offer_work, SdOfferWorkHandler);

static void SdOfferWorkHandler(struct k_work* /*work*/) noexcept
{
    namespace ns = body_control::lighting;

    // Lazy socket creation — deferred until the network stack is up.
    if (g_sd_fd < 0)
    {
        g_sd_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_sd_fd >= 0)
        {
            const int ttl = 1;
            static_cast<void>(setsockopt(g_sd_fd, IPPROTO_IP,
                                         IP_MULTICAST_TTL, &ttl, sizeof(ttl)));
        }
    }

    if (g_sd_fd >= 0)
    {
        ns::transport::ServiceOffer offer {};
        offer.service_id    = ns::domain::exterior_lighting_service::kServiceId;
        offer.instance_id   = ns::domain::exterior_lighting_service::kInstanceId;
        offer.major_version = 1U;
        offer.ttl_seconds   = 5U;
        offer.local_port    = kExteriorLightingNodePort;
        // Advertise the NUCLEO's static unicast IP so the controller can
        // open a direct data socket to this endpoint.
        std::strncpy(offer.local_ip,
                     kCzcIpStr[0] != '\0' ? "192.168.0.20" : "192.168.0.20",
                     sizeof(offer.local_ip) - 1U);

        const auto frame =
            ns::transport::SomeIpSdCodec::EncodeOffer(offer, g_sd_session++);

        struct sockaddr_in mcast {};
        mcast.sin_family = AF_INET;
        mcast.sin_port   = htons(30490U);
        inet_pton(AF_INET, "224.244.224.245", &mcast.sin_addr);

        static_cast<void>(sendto(
            g_sd_fd, frame.data(), frame.size(), 0,
            reinterpret_cast<const struct sockaddr*>(&mcast),
            static_cast<socklen_t>(sizeof(mcast))));

        LOG_INF("[SD] OfferService 0x%04x sent (%zu bytes)",
                static_cast<unsigned>(offer.service_id), frame.size());
    }
    else
    {
        LOG_WRN("[SD] socket not ready, retrying");
    }

    k_work_schedule(&g_sd_offer_work, K_MSEC(2000));
}

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
        body_control::lighting::application::ExteriorLightingFunctionManager& fmgr)
        noexcept
        : gpio_(gpio), fmgr_(fmgr)
    {}

    void ProcessTick(const std::uint32_t now_ms) noexcept
    {
        const bool hazard_on = IsOn(LF::kHazardLamp);

        LOG_DBG("BLINK park=%d head=%d",
            static_cast<int>(IsOn(LF::kParkLamp)),
            static_cast<int>(IsOn(LF::kHeadLamp)));

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
            LOG_DBG("BLINK_TICK hazard=%d left=%d right=%d phase=%d",
                static_cast<int>(IsOn(LF::kHazardLamp)),
                static_cast<int>(IsOn(LF::kLeftIndicator)),
                static_cast<int>(IsOn(LF::kRightIndicator)),
                static_cast<int>(phase));
            WriteGpio(LF::kHazardLamp,     phase, last_written_.hazard);
            WriteGpio(LF::kLeftIndicator,  phase, last_written_.left);
            WriteGpio(LF::kRightIndicator, phase, last_written_.right);
        }
        else
        {
            hazard_blink_ = BlinkState {};
            WriteGpio(LF::kHazardLamp, false, last_written_.hazard);
            DriveIndicator(now_ms, LF::kLeftIndicator,  left_blink_,  last_written_.left);
            DriveIndicator(now_ms, LF::kRightIndicator, right_blink_, last_written_.right);
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

    struct GpioOutputState
    {
        int hazard {-1};
        int left   {-1};
        int right  {-1};
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

    void WriteGpio(LF func, bool state, int& last) noexcept
    {
        const int s {static_cast<int>(state)};
        if (s != last)
        {
            LOG_INF("BLINK lamp=%d state=%d", static_cast<int>(func), s);
            last = s;
        }
        static_cast<void>(gpio_.WriteLampOutput(func, state));
    }

    void DriveIndicator(
        const std::uint32_t now_ms,
        const LF            func,
        BlinkState&         state,
        int&                last_written) noexcept
    {
        if (IsOn(func))
        {
            if (!state.was_active) { state = BlinkState {true, now_ms, true}; }
            else                   { Tick(now_ms, state); }
            WriteGpio(func, state.phase_on, last_written);
        }
        else
        {
            state = BlinkState {};
            WriteGpio(func, false, last_written);
        }
    }

    zgpio::ZephyrGpioDriver&                                          gpio_;
    body_control::lighting::application::ExteriorLightingFunctionManager& fmgr_;
    BlinkState      left_blink_    {};
    BlinkState      right_blink_   {};
    BlinkState      hazard_blink_  {};
    GpioOutputState last_written_  {};
};

// ---- Arbitration helpers ---------------------------------------------------
//
// Ported from ExteriorLightingNodeHandler in the bare-metal main.cpp.
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
    LOG_INF("HAZARD action=%d hazard_on=%d",
        static_cast<int>(cmd.action),
        static_cast<int>(IsOn(LF::kHazardLamp)));

    LOG_INF("DISPATCH lamp=%d action=%d seq=%u",
        static_cast<int>(cmd.function),
        static_cast<int>(cmd.action),
        static_cast<unsigned>(cmd.sequence_counter));

    g_last_hazard_sequence = cmd.sequence_counter;

    const bool hazard_was_on = IsOn(LF::kHazardLamp);

    LC resolved     = cmd;
    resolved.action = hazard_was_on ? LCA::kDeactivate : LCA::kActivate;
    LOG_INF("APPLY lamp=%d action=%d",
        static_cast<int>(resolved.function),
        static_cast<int>(resolved.action));
    static_cast<void>(g_lamp_mgr.ApplyCommand(resolved));
    {
        const bool new_state = IsOn(resolved.function);
        LOG_INF("APPLIED lamp=%d new_state=%d",
            static_cast<int>(resolved.function),
            static_cast<int>(new_state));
    }

    if (hazard_was_on)
    {
        g_indicator_registry.hazard_active = false;

        // Deactivate both indicators, then restore the pre-hazard one.
        LC off      = cmd;
        off.action  = LCA::kDeactivate;
        off.function = LF::kLeftIndicator;
        LOG_INF("APPLY lamp=%d action=%d",
            static_cast<int>(off.function),
            static_cast<int>(off.action));
        static_cast<void>(g_lamp_mgr.ApplyCommand(off));
        {
            const bool new_state = IsOn(off.function);
            LOG_INF("APPLIED lamp=%d new_state=%d",
                static_cast<int>(off.function),
                static_cast<int>(new_state));
        }
        off.function = LF::kRightIndicator;
        LOG_INF("APPLY lamp=%d action=%d",
            static_cast<int>(off.function),
            static_cast<int>(off.action));
        static_cast<void>(g_lamp_mgr.ApplyCommand(off));
        {
            const bool new_state = IsOn(off.function);
            LOG_INF("APPLIED lamp=%d new_state=%d",
                static_cast<int>(off.function),
                static_cast<int>(new_state));
        }
        SendLampStatusEvent(LF::kHazardLamp);
        SendLampStatusEvent(LF::kLeftIndicator);
        SendLampStatusEvent(LF::kRightIndicator);

        if (g_indicator_registry.active_indicator != LF::kUnknown)
        {
            LC restore       = cmd;
            restore.function = g_indicator_registry.active_indicator;
            restore.action   = LCA::kActivate;
            LOG_INF("APPLY lamp=%d action=%d",
                static_cast<int>(restore.function),
                static_cast<int>(restore.action));
            static_cast<void>(g_lamp_mgr.ApplyCommand(restore));
            {
                const bool new_state = IsOn(restore.function);
                LOG_INF("APPLIED lamp=%d new_state=%d",
                    static_cast<int>(restore.function),
                    static_cast<int>(new_state));
            }
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
    LOG_INF("DISPATCH lamp=%d action=%d seq=%u",
        static_cast<int>(cmd.function),
        static_cast<int>(cmd.action),
        static_cast<unsigned>(cmd.sequence_counter));

    // Suppress companion indicator commands sent as part of a hazard fanout.
    if (cmd.sequence_counter == g_last_hazard_sequence)
    {
        LOG_WRN("DROPPED lamp=%d reason=%s",
            static_cast<int>(cmd.function), "hazard_companion_seq");
        return;
    }

    if (IsOn(LF::kHazardLamp))
    {
        LOG_WRN("Indicator blocked: hazard active");
        LOG_WRN("DROPPED lamp=%d reason=%s",
            static_cast<int>(cmd.function), "hazard_active");
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
            LOG_INF("APPLY lamp=%d action=%d",
                static_cast<int>(deactivate.function),
                static_cast<int>(deactivate.action));
            static_cast<void>(g_lamp_mgr.ApplyCommand(deactivate));
            {
                const bool new_state = IsOn(deactivate.function);
                LOG_INF("APPLIED lamp=%d new_state=%d",
                    static_cast<int>(deactivate.function),
                    static_cast<int>(new_state));
            }
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

    LOG_INF("APPLY lamp=%d action=%d",
        static_cast<int>(resolved.function),
        static_cast<int>(resolved.action));
    static_cast<void>(g_lamp_mgr.ApplyCommand(resolved));
    {
        const bool new_state = IsOn(resolved.function);
        LOG_INF("APPLIED lamp=%d new_state=%d",
            static_cast<int>(resolved.function),
            static_cast<int>(new_state));
    }
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
        LOG_INF("DISPATCH lamp=%d action=%d seq=%u",
            static_cast<int>(cmd.function),
            static_cast<int>(cmd.action),
            static_cast<unsigned>(cmd.sequence_counter));

        // Park and head lamps: no arbitration.
        LOG_INF("APPLY lamp=%d action=%d",
            static_cast<int>(cmd.function),
            static_cast<int>(cmd.action));
        const bool apply_ok = g_lamp_mgr.ApplyCommand(cmd);
        {
            const bool new_state = IsOn(cmd.function);
            LOG_INF("APPLIED lamp=%d new_state=%d",
                static_cast<int>(cmd.function),
                static_cast<int>(new_state));
        }
        if (apply_ok)
        {
            SendLampStatusEvent(cmd.function);
            LS result {};
            if (g_lamp_mgr.GetLampStatus(cmd.function, result))
            {
                LOG_INF("Lamp command applied: %s",
                        (result.output_state == LOS::kOn) ? "ON" : "OFF");
            }
        }
        else
        {
            LOG_WRN("DROPPED lamp=%d reason=%s",
                static_cast<int>(cmd.function), "apply_command_rejected");
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

        // FIFO enqueue — do NOT purge.  A hazard press from the CZC fans out
        // into three datagrams ([hazard, left, right]) all carrying the same
        // sequence_counter.  An earlier "latest-wins" purge here destroyed the
        // hazard and left messages every fanout, leaving only the right
        // indicator command in the queue.  cmd_thread then ran HandleIndicator
        // on right (with g_last_hazard_sequence still at its prior value), so
        // hazard never activated in fmgr — only the right indicator did.
        // Mirror the STM32 bare-metal path which processes every datagram in
        // arrival order; queue depth (8) holds two full hazard fanouts and
        // cmd_thread drains in microseconds, so backpressure is bounded.
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
        // k_msgq_get returns 0 on success.  When the dispatcher calls
        // k_msgq_purge while this thread is pended on an empty queue,
        // Zephyr wakes the pended reader with -ENOMSG and does NOT
        // populate the data buffer (see zephyr/kernel/msg_q.c
        // z_impl_k_msgq_purge: arch_thread_return_value_set(..., -ENOMSG)
        // without copying any payload).  If we ignore the return value,
        // ApplyLampCommand(cmd) runs with a STALE buffer — initially the
        // zero-initialised LampCommand (lamp=0 action=0 seq=0, the
        // observed phantom dispatch) and on later iterations the
        // previously-processed command, which manifested as the apparent
        // "kToggle expansion" trace where one RX produced two DISPATCH
        // events (the stale prior command followed by the real new one).
        // Skip dispatch on any non-success return so each RX yields
        // exactly one ApplyCommand call.
        if (k_msgq_get(&g_lamp_cmd_queue, &cmd, K_FOREVER) != 0)
        {
            continue;
        }
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

                // MCUboot image confirmation — called once after the first
                // successful health event transmission.  This proves the
                // network stack, GPIO driver, and dispatch loop are all
                // healthy.  Without this call MCUboot rolls back to the
                // previous image on the next boot (BOOT_UPGRADE_TEST mode).
                static bool s_image_confirmed {false};
                if (!s_image_confirmed)
                {
                    if (boot_write_img_confirmed() == 0)
                    {
                        LOG_INF("[OTA] image confirmed");
                    }
                    s_image_confirmed = true;
                }
            }
        }
    }
}

// ---- DoIP TCP diagnostic server (Phase 13) ---------------------------------
//
// Implements ISO 13400-2 DoIP over Zephyr BSD sockets (TCP port 13400).
// Accepts one tester connection at a time.  Handles:
//   0x0005 RoutingActivationRequest → 0x0006 RoutingActivationResponse
//   0x8001 DiagnosticMessage        → 0x8002 ACK + 0x8001 UDS response
//
// UDS requests are dispatched to g_uds_handler which includes the
// OtaSessionManager.  After a successful 0x37 RequestTransferExit the
// OtaSessionManager schedules a 100 ms delayed reboot; this thread will
// naturally exit when the system resets.
//
// This is a parallel implementation to the LwIP-based DoipTcpServer in
// app/stm32_nucleo_h753zi/main.cpp.  The two cannot share code because
// the STM32 bare-metal build uses the LwIP raw-API callback model (driven
// by the main polling loop) while Zephyr requires a dedicated blocking
// thread on BSD sockets.  The DoIP framing logic (~80 lines) is identical.

namespace
{

// DoIP protocol constants (ISO 13400-2).
static constexpr std::uint16_t kDoipPort               {13400U};
static constexpr std::uint8_t  kDoipProtocolVersion    {0xFDU};
static constexpr std::size_t   kDoipHeaderSize         {8U};
static constexpr std::uint32_t kDoipMaxPayloadLen      {1024U};
static constexpr std::uint16_t kDoipTypeRoutingReq     {0x0005U};
static constexpr std::uint16_t kDoipTypeRoutingResp    {0x0006U};
static constexpr std::uint16_t kDoipTypeDiagMsg        {0x8001U};
static constexpr std::uint16_t kDoipTypeDiagMsgAck     {0x8002U};
static constexpr std::uint16_t kDoipNodeAddr           {0x0E01U};
static constexpr std::uint8_t  kDoipRoutingActivated   {0x10U};

// Receive exactly n bytes from socket fd.  Returns false on disconnect/error.
static bool RecvExact(int fd, std::uint8_t* buf, std::size_t n) noexcept
{
    std::size_t received {0U};
    while (received < n)
    {
        const ssize_t r = recv(fd, buf + received, n - received, 0);
        if (r <= 0) { return false; }
        received += static_cast<std::size_t>(r);
    }
    return true;
}

// Send a complete DoIP frame.  Returns false on error.
static bool SendDoipFrame(
    int                  fd,
    std::uint16_t        ptype,
    const std::uint8_t*  payload,
    std::uint32_t        plen) noexcept
{
    std::uint8_t hdr[kDoipHeaderSize] = {
        kDoipProtocolVersion,
        static_cast<std::uint8_t>(~kDoipProtocolVersion),
        static_cast<std::uint8_t>(ptype >> 8U),
        static_cast<std::uint8_t>(ptype & 0xFFU),
        static_cast<std::uint8_t>(plen >> 24U),
        static_cast<std::uint8_t>(plen >> 16U),
        static_cast<std::uint8_t>(plen >>  8U),
        static_cast<std::uint8_t>(plen  & 0xFFU),
    };

    if (send(fd, hdr, kDoipHeaderSize, 0) !=
        static_cast<ssize_t>(kDoipHeaderSize)) { return false; }

    if (plen > 0U)
    {
        if (send(fd, payload, plen, 0) != static_cast<ssize_t>(plen))
        { return false; }
    }
    return true;
}

static void HandleDoipConnection(int conn_fd) noexcept
{
    bool routing_active {false};
    std::uint8_t hdr[kDoipHeaderSize];
    static std::uint8_t payload_buf[kDoipMaxPayloadLen];

    while (true)
    {
        if (!RecvExact(conn_fd, hdr, kDoipHeaderSize)) { break; }

        if (hdr[0] != kDoipProtocolVersion) { break; }

        const std::uint16_t ptype =
            (static_cast<std::uint16_t>(hdr[2]) << 8U) | hdr[3];
        const std::uint32_t plen  =
            (static_cast<std::uint32_t>(hdr[4]) << 24U) |
            (static_cast<std::uint32_t>(hdr[5]) << 16U) |
            (static_cast<std::uint32_t>(hdr[6]) <<  8U) |
             static_cast<std::uint32_t>(hdr[7]);

        if (plen > kDoipMaxPayloadLen) { break; }

        if (!RecvExact(conn_fd, payload_buf, plen)) { break; }

        if (ptype == kDoipTypeRoutingReq && plen >= 7U)
        {
            const std::uint16_t tester_addr =
                (static_cast<std::uint16_t>(payload_buf[0]) << 8U) | payload_buf[1];

            std::uint8_t resp[9U] = {
                static_cast<std::uint8_t>(tester_addr >> 8U),
                static_cast<std::uint8_t>(tester_addr & 0xFFU),
                static_cast<std::uint8_t>(kDoipNodeAddr >> 8U),
                static_cast<std::uint8_t>(kDoipNodeAddr & 0xFFU),
                kDoipRoutingActivated,
                0U, 0U, 0U, 0U
            };

            if (SendDoipFrame(conn_fd, kDoipTypeRoutingResp, resp, 9U))
            {
                routing_active = true;
                LOG_INF("[DoIP] routing activated");
            }
            else { break; }
        }
        else if (ptype == kDoipTypeDiagMsg && routing_active && plen >= 5U)
        {
            const std::uint16_t src_addr =
                (static_cast<std::uint16_t>(payload_buf[0]) << 8U) | payload_buf[1];
            const std::uint16_t dst_addr =
                (static_cast<std::uint16_t>(payload_buf[2]) << 8U) | payload_buf[3];

            if (dst_addr != kDoipNodeAddr)
            {
                LOG_WRN("[DoIP] wrong target addr 0x%04X", dst_addr);
                continue;
            }

            // Send ACK before dispatching (tester times out without it).
            std::uint8_t ack[5U] = {
                static_cast<std::uint8_t>(kDoipNodeAddr >> 8U),
                static_cast<std::uint8_t>(kDoipNodeAddr & 0xFFU),
                static_cast<std::uint8_t>(src_addr >> 8U),
                static_cast<std::uint8_t>(src_addr & 0xFFU),
                0x00U
            };
            if (!SendDoipFrame(conn_fd, kDoipTypeDiagMsgAck, ack, 5U)) { break; }

            // UDS payload starts at payload_buf[4].
            const std::vector<std::uint8_t> uds_req(
                payload_buf + 4U,
                payload_buf + plen);

            const std::vector<std::uint8_t> uds_resp =
                g_uds_handler.HandleRequest(uds_req);

            // Response: node_addr(2) + src_addr(2) + uds_response bytes.
            static std::uint8_t resp_buf[520U];
            const std::size_t uds_len = uds_resp.size();
            if (uds_len > 516U) { break; }

            resp_buf[0] = static_cast<std::uint8_t>(kDoipNodeAddr >> 8U);
            resp_buf[1] = static_cast<std::uint8_t>(kDoipNodeAddr & 0xFFU);
            resp_buf[2] = static_cast<std::uint8_t>(src_addr >> 8U);
            resp_buf[3] = static_cast<std::uint8_t>(src_addr & 0xFFU);
            std::memcpy(resp_buf + 4U, uds_resp.data(), uds_len);

            if (!SendDoipFrame(conn_fd, kDoipTypeDiagMsg, resp_buf,
                               static_cast<std::uint32_t>(4U + uds_len))) { break; }
        }
    }

    close(conn_fd);
}

}  // namespace

static void DoipThread(void* /*p1*/, void* /*p2*/, void* /*p3*/)
{
    LOG_INF("[DoIP] thread started on TCP port %u", kDoipPort);

    // Poll NET_IF_RUNNING — set by the Ethernet driver only after PHY
    // autonegotiation completes.  net_if_is_up() and IPv4 address assignment
    // both precede PHY link-up on static-IP configs and are false gates.
    // NET_EVENT_L4_CONNECTED requires IPv6 to fire reliably; with
    // CONFIG_NET_IPV6=n it never fires.  The flag poll is IPv4/IPv6 agnostic.
    struct net_if* const iface = net_if_get_default();
    std::int32_t warn_ticks {0};
    while (true)
    {
        if ((iface != nullptr)
         && net_if_is_up(iface)
         && net_if_flag_is_set(iface, NET_IF_RUNNING))
        {
            break;
        }
        k_msleep(100);
        if (++warn_ticks >= 300)
        {
            LOG_WRN("[DoIP] PHY not running after 30 s — check Ethernet cable");
            warn_ticks = 0;
        }
    }
    LOG_INF("[DoIP] PHY link running, opening TCP socket");

    const int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd < 0)
    {
        LOG_ERR("[DoIP] socket() failed: %d", errno);
        return;
    }

    const int opt {1};
    static_cast<void>(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
                                 &opt, sizeof(opt)));

    struct sockaddr_in addr {};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(kDoipPort);

    if (bind(server_fd, reinterpret_cast<const struct sockaddr*>(&addr),
             sizeof(addr)) < 0)
    {
        LOG_ERR("[DoIP] bind() failed: %d", errno);
        close(server_fd);
        return;
    }

    if (listen(server_fd, 1) < 0)
    {
        LOG_ERR("[DoIP] listen() failed: %d", errno);
        close(server_fd);
        return;
    }

    LOG_INF("[DoIP] listening");

    while (true)
    {
        struct sockaddr_in client_addr {};
        socklen_t client_len {sizeof(client_addr)};

        const int conn_fd = accept(
            server_fd,
            reinterpret_cast<struct sockaddr*>(&client_addr),
            &client_len);

        if (conn_fd < 0)
        {
            LOG_WRN("[DoIP] accept() failed: %d", errno);
            continue;
        }

        LOG_INF("[DoIP] tester connected");
        HandleDoipConnection(conn_fd);
        LOG_INF("[DoIP] tester disconnected");
    }
}

// ---- Entry point -----------------------------------------------------------

int main()
{
    LOG_INF("Exterior lighting node starting (Zephyr RTOS)");

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
    LOG_INF("Transport init OK — listening on :%u", kExteriorLightingNodePort);

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

    LOG_INF("Exterior lighting node started");

    // Start SOME/IP-SD OfferService immediately; the work handler retries the
    // socket creation on every tick until the network is up.
    k_work_schedule(&g_sd_offer_work, K_MSEC(0));

    k_thread_create(&g_doip_thread_data,
                    g_doip_stack,
                    K_THREAD_STACK_SIZEOF(g_doip_stack),
                    DoipThread,
                    nullptr, nullptr, nullptr,
                    7, 0, K_NO_WAIT);
    k_thread_name_set(&g_doip_thread_data, "doip");

    // main() returns; the Zephyr idle thread takes over.  All work is in the
    // five spawned threads which run indefinitely.
    return 0;
}

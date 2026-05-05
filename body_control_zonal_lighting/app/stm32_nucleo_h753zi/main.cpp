// STM32 NUCLEO-H753ZI — rear lighting node entry point.
//
// Role: receives SOME/IP lamp commands from the Linux CentralZoneController
// over UDP/LwIP and drives the five GPIO outputs (PB0-PB4).
//
// Network configuration (static):
//   NUCLEO IP  : 192.168.0.20  (kNucleoIpAddr)
//   CZC IP     : 192.168.0.10  (kCzcIpAddr)
//   Local port : 41001         (kRearLightingNodePort)
//   Remote port: 41000         (kCentralZoneControllerPort)

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_it.hpp"

#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/application/uds_request_handler.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_constants.hpp"
#include "body_control/lighting/platform/stm32/ethernet_link_supervisor.hpp"
#include "body_control/lighting/platform/stm32/gpio_output_driver.hpp"
#include "body_control/lighting/platform/stm32/stm32_diagnostic_logger.hpp"
#include "body_control/lighting/transport/lwip/lwip_udp_transport_adapter.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

// Required by newlib's __libc_init_array when using a bare-metal toolchain.
extern "C" void _init(void) {}

// Retarget newlib printf() → USART3 so ETH-TX and pool diagnostics reach the
// serial console.  Mirrors the polling loop in stm32_diagnostic_logger.cpp.
extern "C" int _write(int /*fd*/, const char* const ptr, const int len)
{
    if ((RCC->APB1LENR & RCC_APB1LENR_USART3EN) == 0U) { return 0; }
    if ((USART3->CR1 & USART_CR1_UE) == 0U) { return 0; }
    for (int i = 0; i < len; ++i)
    {
        while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0U) {}
        USART3->TDR = static_cast<std::uint8_t>(ptr[i]);
    }
    while ((USART3->ISR & USART_ISR_TC) == 0U) {}
    return len;
}

// Defined in ethernetif.cpp; called from this main loop.
extern "C"
{
err_t ethernetif_init(struct netif* netif);
void  ethernetif_input(struct netif* netif);
void  ethernetif_poll_phy(struct netif* netif);

// sys_now() is called by LwIP's internal timeout machinery (sys_check_timeouts).
// HAL_GetTick() returns the SysTick millisecond counter, which is the natural
// bare-metal equivalent of a wall-clock millisecond source on Cortex-M.
uint32_t sys_now(void) { return HAL_GetTick(); }
}

extern ETH_HandleTypeDef heth;  // defined in ethernetif.cpp

namespace
{

// ---- Network constants -----------------------------------------------------

constexpr std::uint32_t kNucleoIpAddr  {0xC0A80014U};  // 192.168.0.20
constexpr std::uint32_t kCzcIpAddr     {0xC0A8000AU};  // 192.168.0.10
constexpr std::uint32_t kSubnetMask    {0xFFFFFF00U};  // /24
constexpr std::uint32_t kGatewayAddr   {0xC0A80001U};  // 192.168.0.1

constexpr std::uint16_t kRearLightingNodePort       {41001U};
constexpr std::uint16_t kCentralZoneControllerPort  {41000U};

constexpr std::uint32_t kNodeHealthPublishPeriodMs {
    static_cast<std::uint32_t>(
        body_control::lighting::domain::timing::kNodeHealthPublishPeriod.count())};

// ---- Clock config ----------------------------------------------------------

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init {};
    RCC_ClkInitTypeDef clk_init {};
    RCC_PeriphCLKInitTypeDef periph_clk_init {};

    // NUCLEO-H753ZI uses ST-Link MCO as HSE source (bypass mode, 8 MHz).
    // VOS1 supports SYSCLK up to 400 MHz without requiring CSI/comp-cell.
    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    // HSE 8 MHz (bypass) → PLL1: PLLM=4 → 2 MHz input → ×400 → 800 MHz VCO
    // PLLP=2 → SYSCLK=400 MHz.  HCLK=200 MHz (DIV2).  APBx=100 MHz (DIV2).
    osc_init.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc_init.HSEState       = RCC_HSE_BYPASS;
    osc_init.HSIState       = RCC_HSI_OFF;
    osc_init.CSIState       = RCC_CSI_OFF;
    osc_init.PLL.PLLState   = RCC_PLL_ON;
    osc_init.PLL.PLLSource  = RCC_PLLSOURCE_HSE;
    osc_init.PLL.PLLM       = 4U;
    osc_init.PLL.PLLN       = 400U;
    osc_init.PLL.PLLP       = 2U;
    osc_init.PLL.PLLQ       = 4U;
    osc_init.PLL.PLLR       = 2U;
    osc_init.PLL.PLLRGE     = RCC_PLL1VCIRANGE_1;  // 2-4 MHz VCI input
    osc_init.PLL.PLLVCOSEL  = RCC_PLL1VCOWIDE;
    osc_init.PLL.PLLFRACN   = 0U;
    HAL_RCC_OscConfig(&osc_init);

    clk_init.ClockType      = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                            | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                            | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk_init.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk_init.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    clk_init.AHBCLKDivider  = RCC_HCLK_DIV2;       // HCLK = 200 MHz
    clk_init.APB3CLKDivider = RCC_APB3_DIV2;
    clk_init.APB1CLKDivider = RCC_APB1_DIV2;
    clk_init.APB2CLKDivider = RCC_APB2_DIV2;
    clk_init.APB4CLKDivider = RCC_APB4_DIV2;
    HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_4);

    // Route USART3 clock from D2PCLK1 (= APB1 = 100 MHz) so BRR is predictable.
    periph_clk_init.PeriphClockSelection      = RCC_PERIPHCLK_USART3;
    periph_clk_init.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
    HAL_RCCEx_PeriphCLKConfig(&periph_clk_init);
}

// ---- UART (virtual COM port) init ------------------------------------------

void MX_USART3_UART_Init(void)
{
    UART_HandleTypeDef huart3 {};
    huart3.Instance          = USART3;
    huart3.Init.BaudRate     = 115200U;
    huart3.Init.WordLength   = UART_WORDLENGTH_8B;
    huart3.Init.StopBits     = UART_STOPBITS_1;
    huart3.Init.Parity       = UART_PARITY_NONE;
    huart3.Init.Mode         = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

// ---- Blink manager ---------------------------------------------------------
//
// Owns all GPIO writes for the five lamp outputs.  Called every main-loop
// tick; internal 500 ms phase timers govern when blinking outputs toggle.
//
// Hazard active: PB2 (hazard) and PB0 (left indicator) blink in sync.
//   PB1 (right indicator) is forced low while hazard is active.
// Left/right indicators: blink at kIndicatorOnTime/kIndicatorOffTime when ON.
//   Each indicator starts at phase HIGH the moment it activates — BlinkState
//   is reset to {phase_on=true, start=now} on every OFF→ON transition so
//   the first visible edge is always an ON pulse of full 500 ms duration.
// Park and head lamps: steady on/off — no blinking.

class BlinkManager
{
public:
    explicit BlinkManager(
        body_control::lighting::platform::stm32::GpioOutputDriver&        gpio,
        body_control::lighting::application::RearLightingFunctionManager& fmgr) noexcept
        : gpio_(gpio)
        , fmgr_(fmgr)
    {
    }

    void ProcessMainLoop(const std::uint32_t now_ms) noexcept
    {
        using body_control::lighting::domain::LampFunction;

        const bool hazard_on = IsOn(LampFunction::kHazardLamp);

        if (hazard_on)
        {
            // First entry: start blink from phase ON.
            if (!hazard_blink_.was_active)
            {
                hazard_blink_ = BlinkState {true, now_ms, true};
            }
            else
            {
                Tick(now_ms, hazard_blink_);
            }
            const bool phase = hazard_blink_.phase_on;
            static_cast<void>(gpio_.WriteLampOutput(LampFunction::kHazardLamp,     phase));
            static_cast<void>(gpio_.WriteLampOutput(LampFunction::kLeftIndicator,  phase));
            static_cast<void>(gpio_.WriteLampOutput(LampFunction::kRightIndicator, phase));
        }
        else
        {
            hazard_blink_ = BlinkState {};
            static_cast<void>(gpio_.WriteLampOutput(LampFunction::kHazardLamp, false));

            DriveIndicator(now_ms, LampFunction::kLeftIndicator,  left_blink_);
            DriveIndicator(now_ms, LampFunction::kRightIndicator, right_blink_);
        }

        // Park and head lamps: steady — no phase state needed.
        static_cast<void>(gpio_.WriteLampOutput(
            LampFunction::kParkLamp, IsOn(LampFunction::kParkLamp)));
        static_cast<void>(gpio_.WriteLampOutput(
            LampFunction::kHeadLamp, IsOn(LampFunction::kHeadLamp)));
    }

private:
    struct BlinkState
    {
        bool          phase_on       {false};
        std::uint32_t phase_start_ms {0U};
        bool          was_active     {false};
    };

    bool IsOn(const body_control::lighting::domain::LampFunction func) const noexcept
    {
        body_control::lighting::domain::LampStatus status {};
        fmgr_.GetLampStatus(func, status);
        return status.output_state ==
               body_control::lighting::domain::LampOutputState::kOn;
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
        const std::uint32_t                              now_ms,
        const body_control::lighting::domain::LampFunction func,
        BlinkState&                                      state) noexcept
    {
        if (IsOn(func))
        {
            if (!state.was_active)
            {
                // OFF→ON transition: begin with a full ON pulse from now.
                state = BlinkState {true, now_ms, true};
            }
            else
            {
                Tick(now_ms, state);
            }
            static_cast<void>(gpio_.WriteLampOutput(func, state.phase_on));
        }
        else
        {
            state = BlinkState {};
            static_cast<void>(gpio_.WriteLampOutput(func, false));
        }
    }

    body_control::lighting::platform::stm32::GpioOutputDriver&        gpio_;
    body_control::lighting::application::RearLightingFunctionManager& fmgr_;
    BlinkState left_blink_   {};
    BlinkState right_blink_  {};
    BlinkState hazard_blink_ {};
};

// ---- Rear lighting node message handler ------------------------------------
//
// Applies local arbitration so the STM32 is self-sufficient.  The Linux CZC
// uses LampStateManager to build its ArbitrationContext, but that state is
// never updated (the NUCLEO sends no status responses), so the CZC context
// is always stale.  The handler therefore:
//
//   Hazard:     Always toggles based on the NUCLEO's own fmgr state,
//               ignoring whether the incoming action is kActivate/kDeactivate.
//               When hazard deactivates, both indicator states are cleared.
//
//   Indicators: Resolve kToggle to kActivate/kDeactivate from fmgr state.
//               Enforce exclusivity: activating one side deactivates the other.
//               Block while hazard is active.
//
//   Park/head:  Applied as received — no arbitration needed.

class RearLightingNodeHandler final
    : public body_control::lighting::transport::TransportMessageHandlerInterface
{
    using LF  = body_control::lighting::domain::LampFunction;
    using LCA = body_control::lighting::domain::LampCommandAction;
    using LOS = body_control::lighting::domain::LampOutputState;
    using LC  = body_control::lighting::domain::LampCommand;
    using LS  = body_control::lighting::domain::LampStatus;

public:
    explicit RearLightingNodeHandler(
        body_control::lighting::application::RearLightingFunctionManager& fmgr,
        body_control::lighting::platform::stm32::Stm32DiagnosticLogger&   logger,
        body_control::lighting::transport::TransportAdapterInterface&      transport) noexcept
        : fmgr_(fmgr)
        , logger_(logger)
        , transport_(transport)
    {
    }

    void OnTransportMessageReceived(
        const body_control::lighting::transport::TransportMessage& msg) override
    {
        using body_control::lighting::transport::SomeipMessageParser;

        if (!SomeipMessageParser::IsSetLampCommandRequest(msg)) { return; }

        const auto cmd = SomeipMessageParser::ParseLampCommand(msg);

        if (cmd.function == LF::kHazardLamp)
        {
            HandleHazard(cmd);
        }
        else if (cmd.function == LF::kLeftIndicator
              || cmd.function == LF::kRightIndicator)
        {
            HandleIndicator(cmd);
        }
        else
        {
            // Park lamp / head lamp: no arbitration, apply as received.
            if (fmgr_.ApplyCommand(cmd))
            {
                SendLampStatusEvent(cmd.function);
                LogResult(cmd.function);
            }
        }
    }

    void OnTransportAvailabilityChanged(const bool is_available) override
    {
        if (is_available) { logger_.LogInfo("Transport up"); }
        else              { logger_.LogWarning("Transport down"); }
    }

    void PublishNodeHealth() noexcept
    {
        using body_control::lighting::domain::NodeHealthStatus;
        using body_control::lighting::domain::NodeHealthState;
        using body_control::lighting::transport::SomeipMessageBuilder;

        NodeHealthStatus health {};
        health.health_state              = NodeHealthState::kOperational;
        health.ethernet_link_available   = true;
        health.service_available         = true;
        health.lamp_driver_fault_present = false;
        health.active_fault_count        = 0U;

        const auto msg = SomeipMessageBuilder::BuildNodeHealthEvent(health);
        static_cast<void>(transport_.SendEvent(msg));
    }

private:
    // Hazard is always a toggle regardless of the incoming action.
    // The CZC resolves kToggle using its own stale state (no feedback from
    // the NUCLEO), so it always sends kActivate when it thinks hazard is off.
    // Using the NUCLEO's own fmgr state as truth is the correct fix.
    void HandleHazard(const LC& cmd) noexcept
    {
        // Record sequence counter so companion indicator commands (sent by the
        // CZC as part of the same hazard fanout) can be suppressed below.
        last_hazard_sequence_ = cmd.sequence_counter;

        const bool hazard_was_on = IsOn(LF::kHazardLamp);

        LC resolved     = cmd;
        resolved.action = hazard_was_on ? LCA::kDeactivate : LCA::kActivate;
        static_cast<void>(fmgr_.ApplyCommand(resolved));

        if (!hazard_was_on)
        {
            // Hazard turning ON — save whichever indicator is active so it can
            // be restored when hazard deactivates (turn-signal retention).
            if      (IsOn(LF::kLeftIndicator))  { active_indicator_before_hazard_ = LF::kLeftIndicator;  }
            else if (IsOn(LF::kRightIndicator)) { active_indicator_before_hazard_ = LF::kRightIndicator; }
            else                                { active_indicator_before_hazard_ = LF::kUnknown;        }

            SendLampStatusEvent(LF::kHazardLamp);
            logger_.LogInfo("Hazard: ON");
        }
        else
        {
            // Hazard turning OFF — restore the saved indicator if one was active
            // before hazard; otherwise clear both.
            // Companion kLeft/kRight deactivate commands from the CZC fanout will
            // be suppressed by the last_hazard_sequence_ guard in HandleIndicator.
            if (active_indicator_before_hazard_ != LF::kUnknown)
            {
                LC restore      = cmd;
                restore.function = active_indicator_before_hazard_;
                restore.action   = LCA::kActivate;
                static_cast<void>(fmgr_.ApplyCommand(restore));

                const LF opposite =
                    (active_indicator_before_hazard_ == LF::kLeftIndicator)
                        ? LF::kRightIndicator : LF::kLeftIndicator;
                LC clr      = cmd;
                clr.function = opposite;
                clr.action   = LCA::kDeactivate;
                static_cast<void>(fmgr_.ApplyCommand(clr));

                SendLampStatusEvent(active_indicator_before_hazard_);
                SendLampStatusEvent(opposite);
                active_indicator_before_hazard_ = LF::kUnknown;
            }
            else
            {
                LC off       = cmd;
                off.action   = LCA::kDeactivate;
                off.function = LF::kLeftIndicator;
                static_cast<void>(fmgr_.ApplyCommand(off));
                off.function = LF::kRightIndicator;
                static_cast<void>(fmgr_.ApplyCommand(off));
                SendLampStatusEvent(LF::kLeftIndicator);
                SendLampStatusEvent(LF::kRightIndicator);
            }

            SendLampStatusEvent(LF::kHazardLamp);
            logger_.LogInfo("Hazard: OFF");
        }
    }

    void HandleIndicator(const LC& cmd) noexcept
    {
        // The CZC hazard fanout sends [kHazard, kLeft, kRight] with the same
        // sequence_counter for both hazard-on and hazard-off.  After HandleHazard
        // toggles hazard state, these companion indicator commands must be dropped
        // — otherwise they activate indicators immediately after hazard turns off.
        if (cmd.sequence_counter == last_hazard_sequence_)
        {
            return;
        }

        // Block while hazard is active.
        if (IsOn(LF::kHazardLamp))
        {
            logger_.LogWarning("Indicator blocked: hazard active");
            return;
        }

        // Resolve kToggle from fmgr state so the action is always explicit.
        LC resolved = cmd;
        if (cmd.action == LCA::kToggle)
        {
            resolved.action = IsOn(cmd.function) ? LCA::kDeactivate
                                                  : LCA::kActivate;
        }

        // Exclusivity: activating one indicator deactivates the opposite.
        if (resolved.action == LCA::kActivate)
        {
            const LF opposite = (cmd.function == LF::kLeftIndicator)
                                    ? LF::kRightIndicator
                                    : LF::kLeftIndicator;
            if (IsOn(opposite))
            {
                LC deactivate    = resolved;
                deactivate.function = opposite;
                deactivate.action   = LCA::kDeactivate;
                static_cast<void>(fmgr_.ApplyCommand(deactivate));
                SendLampStatusEvent(opposite);
            }
        }

        static_cast<void>(fmgr_.ApplyCommand(resolved));
        SendLampStatusEvent(cmd.function);
        LogResult(cmd.function);
    }

    void SendLampStatusEvent(const LF func) noexcept
    {
        using body_control::lighting::transport::SomeipMessageBuilder;
        LS status {};
        if (fmgr_.GetLampStatus(func, status))
        {
            const auto msg = SomeipMessageBuilder::BuildLampStatusEvent(status);
            static_cast<void>(transport_.SendEvent(msg));
        }
    }

    bool IsOn(const LF func) const noexcept
    {
        LS status {};
        fmgr_.GetLampStatus(func, status);
        return status.output_state == LOS::kOn;
    }

    void LogResult(const LF func) noexcept
    {
        LS result {};
        if (fmgr_.GetLampStatus(func, result))
        {
            const bool is_on = (result.output_state == LOS::kOn);
            logger_.LogInfo(is_on ? "Lamp command applied: ON"
                                  : "Lamp command applied: OFF");
        }
    }

    body_control::lighting::application::RearLightingFunctionManager& fmgr_;
    body_control::lighting::platform::stm32::Stm32DiagnosticLogger&   logger_;
    body_control::lighting::transport::TransportAdapterInterface&      transport_;
    // Sequence counter of the last hazard command; indicator commands sharing
    // this counter are companion fanout commands and must be suppressed.
    std::uint16_t last_hazard_sequence_        {0U};
    // Indicator that was active just before hazard activated; kUnknown if none.
    // Restored when hazard deactivates (turn-signal retention — BUG 2 fix).
    LF            active_indicator_before_hazard_ {LF::kUnknown};
};

// ── DoIP TCP server (LwIP raw API, NO_SYS mode) ──────────────────────────────
//
// Listens on TCP port 13400 (ISO 13400-2 DoIP diagnostic port).  Accepts one
// tester connection at a time.  Handles routing activation (0x0005 → 0x0006)
// and diagnostic messages (0x8001 → 0x8002 ACK + 0x8001 UDS response).
//
// All LwIP raw-API callbacks (accept, recv, err) are static members using the
// void* arg to recover the server instance pointer.  This is standard practice
// with NO_SYS LwIP and is safe because the single DoipTcpServer instance lives
// for the entire program lifetime (main() never returns).
//
// TCP processing is driven automatically: ethernetif_input() → netif->input()
// → ethernet_input() → ip4_input() → tcp_input() — no extra main-loop call needed.
// sys_check_timeouts() handles TCP retransmit and keepalive timers.

class DoipTcpServer
{
public:
    explicit DoipTcpServer(
        body_control::lighting::application::UdsRequestHandler& uds_handler) noexcept
        : uds_handler_ {uds_handler}
    {
    }

    // Bind the listener PCB.  Must be called after lwip_init().
    void Init() noexcept
    {
        struct tcp_pcb* pcb = tcp_new();
        if (pcb == nullptr) { return; }

        if (tcp_bind(pcb, IP4_ADDR_ANY, kDoipPort) != ERR_OK)
        {
            tcp_abort(pcb);
            return;
        }

        // tcp_listen frees pcb and returns a new listen PCB (or nullptr on OOM).
        struct tcp_pcb* lpcb = tcp_listen(pcb);
        if (lpcb == nullptr) { return; }

        listen_pcb_ = lpcb;
        tcp_arg(listen_pcb_, this);
        tcp_accept(listen_pcb_, AcceptCallback);
    }

private:
    // ── LwIP callbacks ───────────────────────────────────────────────────────

    static err_t AcceptCallback(
        void* arg, struct tcp_pcb* new_pcb, err_t err) noexcept
    {
        auto* self = static_cast<DoipTcpServer*>(arg);

        if (err != ERR_OK || new_pcb == nullptr) { return ERR_VAL; }

        if (self->conn_pcb_ != nullptr)
        {
            // Reject second connection — DoIP diagnostic supports one tester.
            tcp_abort(new_pcb);
            return ERR_ABRT;
        }

        self->conn_pcb_       = new_pcb;
        self->rx_len_         = 0U;
        self->routing_active_ = false;

        tcp_arg(new_pcb,  self);
        tcp_recv(new_pcb, RecvCallback);
        tcp_err(new_pcb,  ErrCallback);
        tcp_nagle_disable(new_pcb);  // send responses without delay

        return ERR_OK;
    }

    static err_t RecvCallback(
        void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err) noexcept
    {
        auto* self = static_cast<DoipTcpServer*>(arg);

        if (p == nullptr || err != ERR_OK)
        {
            self->CloseConn();
            return ERR_OK;
        }

        // Accumulate into rx_buf_, capped at buffer size.
        const std::uint32_t space =
            static_cast<std::uint32_t>(kRxBufSize) - self->rx_len_;
        const std::uint32_t copy_len =
            std::min(static_cast<std::uint32_t>(p->tot_len), space);

        pbuf_copy_partial(p, self->rx_buf_ + self->rx_len_,
                          static_cast<std::uint16_t>(copy_len), 0U);
        self->rx_len_ += copy_len;
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);

        self->ProcessFrames();
        return ERR_OK;
    }

    static void ErrCallback(void* arg, err_t /*err*/) noexcept
    {
        auto* self = static_cast<DoipTcpServer*>(arg);
        // LwIP already closed the PCB; just clear our pointer.
        self->conn_pcb_       = nullptr;
        self->rx_len_         = 0U;
        self->routing_active_ = false;
    }

    // ── Frame processing ─────────────────────────────────────────────────────

    void CloseConn() noexcept
    {
        if (conn_pcb_ != nullptr)
        {
            tcp_arg(conn_pcb_,  nullptr);
            tcp_recv(conn_pcb_, nullptr);
            tcp_err(conn_pcb_,  nullptr);
            tcp_close(conn_pcb_);
            conn_pcb_ = nullptr;
        }
        rx_len_         = 0U;
        routing_active_ = false;
    }

    void ProcessFrames() noexcept
    {
        while (rx_len_ >= kHeaderSize)
        {
            if (rx_buf_[0] != kProtocolVersion) { CloseConn(); return; }

            const std::uint16_t ptype =
                (static_cast<std::uint16_t>(rx_buf_[2]) << 8U) | rx_buf_[3];
            const std::uint32_t plen  =
                (static_cast<std::uint32_t>(rx_buf_[4]) << 24U) |
                (static_cast<std::uint32_t>(rx_buf_[5]) << 16U) |
                (static_cast<std::uint32_t>(rx_buf_[6]) <<  8U) |
                 static_cast<std::uint32_t>(rx_buf_[7]);

            if (plen > kMaxPayloadLen) { CloseConn(); return; }

            if (rx_len_ < kHeaderSize + plen) { return; }  // wait for more data

            HandleFrame(ptype, rx_buf_ + kHeaderSize, plen);

            const std::uint32_t frame_len = kHeaderSize + plen;
            std::memmove(rx_buf_, rx_buf_ + frame_len, rx_len_ - frame_len);
            rx_len_ -= frame_len;
        }
    }

    void HandleFrame(
        const std::uint16_t  ptype,
        const std::uint8_t*  payload,
        const std::uint32_t  plen) noexcept
    {
        switch (ptype)
        {
        case kTypeRoutingActivReq:
            HandleRoutingActivation(payload, plen);
            break;
        case kTypeDiagMsg:
            if (routing_active_) { HandleDiagnosticMessage(payload, plen); }
            break;
        default:
            break;  // ignore unsupported payload types
        }
    }

    void HandleRoutingActivation(
        const std::uint8_t* payload,
        const std::uint32_t plen) noexcept
    {
        if (plen < 7U) { CloseConn(); return; }

        const std::uint16_t tester_addr =
            (static_cast<std::uint16_t>(payload[0]) << 8U) | payload[1];

        // Response: tester_addr(2) + node_addr(2) + code(1) + reserved(4) = 9 B
        const std::uint8_t resp[9U] = {
            static_cast<std::uint8_t>(tester_addr >> 8U),
            static_cast<std::uint8_t>(tester_addr & 0xFFU),
            static_cast<std::uint8_t>(kNodeAddr >> 8U),
            static_cast<std::uint8_t>(kNodeAddr & 0xFFU),
            kRoutingActivated,
            0U, 0U, 0U, 0U
        };

        if (SendFrame(kTypeRoutingActivResp, resp, 9U)) { routing_active_ = true; }
    }

    void HandleDiagnosticMessage(
        const std::uint8_t* payload,
        const std::uint32_t plen) noexcept
    {
        if (plen < 5U) { return; }

        const std::uint16_t src_addr =
            (static_cast<std::uint16_t>(payload[0]) << 8U) | payload[1];
        const std::uint16_t dst_addr =
            (static_cast<std::uint16_t>(payload[2]) << 8U) | payload[3];

        if (dst_addr != kNodeAddr) { return; }

        // Send positive diagnostic message ACK.
        const std::uint8_t ack[5U] = {
            static_cast<std::uint8_t>(kNodeAddr >> 8U),
            static_cast<std::uint8_t>(kNodeAddr & 0xFFU),
            static_cast<std::uint8_t>(src_addr >> 8U),
            static_cast<std::uint8_t>(src_addr & 0xFFU),
            0x00U
        };
        if (!SendFrame(kTypeDiagMsgAck, ack, 5U)) { return; }

        // Dispatch UDS request.
        const std::vector<std::uint8_t> uds_req(
            payload + 4U, payload + static_cast<std::size_t>(plen));
        const std::vector<std::uint8_t> uds_resp =
            uds_handler_.HandleRequest(uds_req);

        // Build response payload: node_addr(2) + src_addr(2) + uds_response.
        static std::uint8_t resp_buf[512U];
        const std::size_t   uds_len = uds_resp.size();
        if (uds_len > 508U) { return; }  // sanity guard

        resp_buf[0] = static_cast<std::uint8_t>(kNodeAddr >> 8U);
        resp_buf[1] = static_cast<std::uint8_t>(kNodeAddr & 0xFFU);
        resp_buf[2] = static_cast<std::uint8_t>(src_addr >> 8U);
        resp_buf[3] = static_cast<std::uint8_t>(src_addr & 0xFFU);
        std::memcpy(resp_buf + 4U, uds_resp.data(), uds_len);

        SendFrame(kTypeDiagMsg, resp_buf,
                  static_cast<std::uint32_t>(4U + uds_len));
    }

    // Returns false and closes connection on LwIP error.
    bool SendFrame(
        const std::uint16_t  ptype,
        const std::uint8_t*  payload,
        const std::uint32_t  plen) noexcept
    {
        if (conn_pcb_ == nullptr) { return false; }

        std::uint8_t hdr[kHeaderSize] = {
            kProtocolVersion,
            static_cast<std::uint8_t>(~kProtocolVersion),
            static_cast<std::uint8_t>(ptype >> 8U),
            static_cast<std::uint8_t>(ptype  & 0xFFU),
            static_cast<std::uint8_t>(plen >> 24U),
            static_cast<std::uint8_t>(plen >> 16U),
            static_cast<std::uint8_t>(plen >>  8U),
            static_cast<std::uint8_t>(plen  & 0xFFU),
        };

        if (tcp_write(conn_pcb_, hdr,
                      static_cast<std::uint16_t>(kHeaderSize),
                      TCP_WRITE_FLAG_COPY | TCP_WRITE_FLAG_MORE) != ERR_OK)
        {
            CloseConn(); return false;
        }
        if (plen > 0U)
        {
            if (tcp_write(conn_pcb_, payload,
                          static_cast<std::uint16_t>(plen),
                          TCP_WRITE_FLAG_COPY) != ERR_OK)
            {
                CloseConn(); return false;
            }
        }
        tcp_output(conn_pcb_);
        return true;
    }

    // ── Protocol constants ───────────────────────────────────────────────────
    static constexpr std::uint16_t kDoipPort            {13400U};
    static constexpr std::uint8_t  kProtocolVersion     {0xFDU};
    static constexpr std::size_t   kHeaderSize          {8U};
    static constexpr std::uint32_t kMaxPayloadLen       {1024U};
    static constexpr std::size_t   kRxBufSize           {1024U};

    static constexpr std::uint16_t kTypeRoutingActivReq  {0x0005U};
    static constexpr std::uint16_t kTypeRoutingActivResp {0x0006U};
    static constexpr std::uint16_t kTypeDiagMsg          {0x8001U};
    static constexpr std::uint16_t kTypeDiagMsgAck       {0x8002U};
    static constexpr std::uint16_t kNodeAddr             {0x0E01U};
    static constexpr std::uint8_t  kRoutingActivated     {0x10U};

    body_control::lighting::application::UdsRequestHandler& uds_handler_;
    struct tcp_pcb*  listen_pcb_     {nullptr};
    struct tcp_pcb*  conn_pcb_       {nullptr};
    std::uint8_t     rx_buf_[kRxBufSize] {};
    std::uint32_t    rx_len_         {0U};
    bool             routing_active_ {false};
};

}  // namespace

// ---- Entry point -----------------------------------------------------------

int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

    body_control::lighting::platform::stm32::Stm32DiagnosticLogger logger {};
    static_cast<void>(logger.Initialize());
    logger.LogInfo("Boot: clocks+UART up");

    // --- LwIP ---------------------------------------------------------------
    lwip_init();
    logger.LogInfo("Boot: lwip_init done");

    struct netif gnetif {};
    ip_addr_t ip_addr  {};
    ip_addr_t net_mask {};
    ip_addr_t gw_addr  {};

    IP4_ADDR(&ip_addr,
             static_cast<std::uint8_t>((kNucleoIpAddr >> 24U) & 0xFFU),
             static_cast<std::uint8_t>((kNucleoIpAddr >> 16U) & 0xFFU),
             static_cast<std::uint8_t>((kNucleoIpAddr >>  8U) & 0xFFU),
             static_cast<std::uint8_t>( kNucleoIpAddr         & 0xFFU));

    IP4_ADDR(&net_mask,
             static_cast<std::uint8_t>((kSubnetMask >> 24U) & 0xFFU),
             static_cast<std::uint8_t>((kSubnetMask >> 16U) & 0xFFU),
             static_cast<std::uint8_t>((kSubnetMask >>  8U) & 0xFFU),
             static_cast<std::uint8_t>( kSubnetMask         & 0xFFU));

    IP4_ADDR(&gw_addr,
             static_cast<std::uint8_t>((kGatewayAddr >> 24U) & 0xFFU),
             static_cast<std::uint8_t>((kGatewayAddr >> 16U) & 0xFFU),
             static_cast<std::uint8_t>((kGatewayAddr >>  8U) & 0xFFU),
             static_cast<std::uint8_t>( kGatewayAddr         & 0xFFU));

    netif_add(&gnetif, &ip_addr, &net_mask, &gw_addr,
              nullptr, ethernetif_init, ethernet_input);
    netif_set_default(&gnetif);
    netif_set_up(&gnetif);

    // --- Platform drivers ---------------------------------------------------
    body_control::lighting::platform::stm32::GpioOutputDriver gpio_driver {};
    static_cast<void>(gpio_driver.Initialize());

    body_control::lighting::platform::stm32::EthernetLinkSupervisor link_supervisor {};

    // --- Application layer --------------------------------------------------
    body_control::lighting::application::RearLightingFunctionManager function_manager {};

    BlinkManager blink_manager {gpio_driver, function_manager};

    // --- Transport ----------------------------------------------------------
    // remote_ip = CZC host, local_port = rear node port, remote_port = CZC port.
    // LwipUdpTransportAdapter sends events to remote and receives commands from it.
    const body_control::lighting::transport::lwip::LwipUdpConfig udp_config {
        kCzcIpAddr,
        kRearLightingNodePort,
        kCentralZoneControllerPort
    };

    body_control::lighting::transport::lwip::LwipUdpTransportAdapter transport {udp_config};

    RearLightingNodeHandler handler {function_manager, logger, transport};
    transport.SetMessageHandler(&handler);

    static_cast<void>(transport.Initialize());

    // --- DoIP TCP diagnostic server -----------------------------------------
    // UdsRequestHandler wraps function_manager + FaultManager for UDS services.
    // DoipTcpServer binds on TCP 13400; TCP processing is driven by the LwIP
    // stack inside ethernetif_input() — no extra main-loop call required.
    body_control::lighting::application::UdsRequestHandler uds_handler {function_manager};
    DoipTcpServer doip_server {uds_handler};
    doip_server.Init();

    logger.LogInfo("Rear lighting node started");

    // --- Main loop ----------------------------------------------------------
    std::uint32_t last_tick      = HAL_GetTick();
    std::uint32_t last_phy_ms    = HAL_GetTick();
    std::uint32_t last_health_ms = HAL_GetTick();
    std::uint32_t last_stats_ms  = HAL_GetTick();

    while (true)
    {
        ethernetif_input(&gnetif);
        sys_check_timeouts();

        const std::uint32_t now   = HAL_GetTick();
        const std::uint32_t delta = now - last_tick;
        last_tick = now;

        if ((now - last_phy_ms) >= 500U)
        {
            last_phy_ms = now;
            ethernetif_poll_phy(&gnetif);
        }

        if (netif_is_link_up(&gnetif) && (now - last_health_ms) >= kNodeHealthPublishPeriodMs)
        {
            last_health_ms = now;
            handler.PublishNodeHealth();
        }

        if ((now - last_stats_ms) >= 10000U)
        {
            last_stats_ms = now;
            printf("memp PBUF_POOL: %d available\r\n",
                   static_cast<int>(MEMP_STATS_GET(avail, MEMP_PBUF_POOL)));
        }

        // ARP aging (etharp_tmr) and IP reassembly (ip_reass_tmr) are both
        // registered as cyclic timers in lwip_cyclic_timers[] (timeouts.c) and
        // are driven by sys_check_timeouts() above.  Calling them explicitly
        // here would double-increment ARP entry ctime and cause stable entries
        // to expire at ARP_MAXAGE/2 wall-clock seconds — the bug fixed in the
        // commit that removed this block.

        // BlinkManager owns all five GPIO outputs each tick.
        blink_manager.ProcessMainLoop(now);

        const auto elapsed = std::chrono::milliseconds {delta};
        link_supervisor.UpdateRawLinkState(netif_is_link_up(&gnetif) != 0U);
        link_supervisor.ProcessMainLoop(elapsed);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// OTA firmware update — integration point (Phase 12)
//
// Production path: add a DoIP server or a dedicated UDS-over-UDP receiver
// thread; wire incoming UDS 0x34/0x36/0x37 frames to OtaSessionManager.
//
//   OtaSessionManager ota_mgr;
//
//   // On 0x34 RequestDownload:
//   auto resp = ota_mgr.HandleRequestDownload(request);
//   logger.Log("[OTA] entering update mode");
//
//   // On 0x36 TransferData:
//   auto resp = ota_mgr.HandleTransferData(request);
//   logger.Log("[OTA] received block N");
//
//   // On 0x37 RequestTransferExit:
//   auto resp = ota_mgr.HandleRequestTransferExit(request);
//   logger.Log("[OTA] transfer complete — flash write requires bootloader");
//
// Note: Actual flash programming requires STM32 bootloader integration
// (e.g. UART/Ethernet bootloader or MCUboot via OpenAMP).  The UDS protocol
// flow is fully implemented and validated on the Linux simulator.
// ─────────────────────────────────────────────────────────────────────────────

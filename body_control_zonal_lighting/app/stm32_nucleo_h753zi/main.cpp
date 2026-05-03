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

#include <chrono>
#include <cstring>

#include "stm32h7xx_hal.h"
#include "stm32h7xx_it.hpp"

#include "lwip/init.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "netif/ethernet.h"

#include "body_control/lighting/application/command_arbitrator.hpp"
#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
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
        last_hazard_sequence_ = cmd.sequence_counter;

        const bool hazard_was_on = IsOn(LF::kHazardLamp);

        LC resolved     = cmd;
        resolved.action = hazard_was_on ? LCA::kDeactivate : LCA::kActivate;
        static_cast<void>(fmgr_.ApplyCommand(resolved));

        if (hazard_was_on)
        {
            indicator_registry_.hazard_active = false;

            // Deactivate both indicators, then restore the pre-hazard one.
            LC off      = cmd;
            off.action  = LCA::kDeactivate;
            off.function = LF::kLeftIndicator;
            static_cast<void>(fmgr_.ApplyCommand(off));
            off.function = LF::kRightIndicator;
            static_cast<void>(fmgr_.ApplyCommand(off));
            SendLampStatusEvent(LF::kHazardLamp);
            SendLampStatusEvent(LF::kLeftIndicator);
            SendLampStatusEvent(LF::kRightIndicator);

            if (indicator_registry_.active_indicator != LF::kUnknown)
            {
                LC restore       = cmd;
                restore.function = indicator_registry_.active_indicator;
                restore.action   = LCA::kActivate;
                static_cast<void>(fmgr_.ApplyCommand(restore));
                SendLampStatusEvent(indicator_registry_.active_indicator);
                indicator_registry_.active_indicator = LF::kUnknown;
            }

            logger_.LogInfo("Hazard: OFF");
        }
        else
        {
            // Capture which indicator was active before hazard overrides it.
            if      (IsOn(LF::kLeftIndicator))  { indicator_registry_.active_indicator = LF::kLeftIndicator;  }
            else if (IsOn(LF::kRightIndicator)) { indicator_registry_.active_indicator = LF::kRightIndicator; }
            else                                { indicator_registry_.active_indicator = LF::kUnknown;         }
            indicator_registry_.hazard_active = true;

            SendLampStatusEvent(LF::kHazardLamp);
            logger_.LogInfo("Hazard: ON");
        }
    }

    void HandleIndicator(const LC& cmd) noexcept
    {
        // Suppress companion indicator commands sent as part of a hazard fanout.
        if (cmd.sequence_counter == last_hazard_sequence_)
        {
            return;
        }

        if (IsOn(LF::kHazardLamp))
        {
            logger_.LogWarning("Indicator blocked: hazard active");
            return;
        }

        LC resolved = cmd;
        if (cmd.action == LCA::kToggle)
        {
            resolved.action = IsOn(cmd.function) ? LCA::kDeactivate
                                                  : LCA::kActivate;
        }

        if (resolved.action == LCA::kActivate)
        {
            // Record operator intent so hazard-off can restore this indicator.
            indicator_registry_.active_indicator = cmd.function;

            const LF opposite = (cmd.function == LF::kLeftIndicator)
                                    ? LF::kRightIndicator
                                    : LF::kLeftIndicator;
            if (IsOn(opposite))
            {
                LC deactivate       = resolved;
                deactivate.function = opposite;
                deactivate.action   = LCA::kDeactivate;
                static_cast<void>(fmgr_.ApplyCommand(deactivate));
                SendLampStatusEvent(opposite);
            }
        }
        else if (resolved.action == LCA::kDeactivate)
        {
            // Stalk to centre: clear registry so hazard-off won't restore.
            if (indicator_registry_.active_indicator == cmd.function)
            {
                indicator_registry_.active_indicator = LF::kUnknown;
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
    std::uint16_t last_hazard_sequence_ {0U};
    body_control::lighting::application::IndicatorInputRegistry indicator_registry_ {};
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

    logger.LogInfo("Rear lighting node started");

    // --- Main loop ----------------------------------------------------------
    std::uint32_t last_tick      = HAL_GetTick();
    std::uint32_t last_phy_ms    = HAL_GetTick();
    std::uint32_t last_health_ms = HAL_GetTick();

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

        if ((now - last_health_ms) >= kNodeHealthPublishPeriodMs)
        {
            last_health_ms = now;
            handler.PublishNodeHealth();
        }

        // BlinkManager owns all five GPIO outputs each tick.
        blink_manager.ProcessMainLoop(now);

        const auto elapsed = std::chrono::milliseconds {delta};
        link_supervisor.UpdateRawLinkState(netif_is_link_up(&gnetif) != 0U);
        link_supervisor.ProcessMainLoop(elapsed);
    }
}

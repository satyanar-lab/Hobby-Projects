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

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/platform/stm32/ethernet_link_supervisor.hpp"
#include "body_control/lighting/platform/stm32/gpio_output_driver.hpp"
#include "body_control/lighting/platform/stm32/stm32_diagnostic_logger.hpp"
#include "body_control/lighting/transport/lwip/lwip_udp_transport_adapter.hpp"
#include "body_control/lighting/transport/someip_message_parser.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

// Defined in ethernetif.cpp; called from this main loop.
extern "C"
{
err_t  ethernetif_init(struct netif* netif);
void   ethernetif_input(struct netif* netif);

// LwIP NO_SYS=1 requires sys_now() returning a millisecond tick count.
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

// ---- Clock config ----------------------------------------------------------

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef osc_init {};
    RCC_ClkInitTypeDef clk_init {};

    HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);
    while (!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

    // HSE 8 MHz → PLL1 → 480 MHz SYSCLK.
    osc_init.OscillatorType    = RCC_OSCILLATORTYPE_HSE;
    osc_init.HSEState          = RCC_HSE_ON;
    osc_init.PLL.PLLState      = RCC_PLL_ON;
    osc_init.PLL.PLLSource     = RCC_PLLSOURCE_HSE;
    osc_init.PLL.PLLM          = 2U;
    osc_init.PLL.PLLN          = 240U;
    osc_init.PLL.PLLP          = 2U;
    osc_init.PLL.PLLQ          = 4U;
    osc_init.PLL.PLLR          = 2U;
    osc_init.PLL.PLLRGE        = RCC_PLL1VCIRANGE_2;   // VCI = 4 MHz input
    osc_init.PLL.PLLVCOSEL     = RCC_PLL1VCOWIDE;      // VCO = 960 MHz
    osc_init.PLL.PLLFRACN      = 0U;
    HAL_RCC_OscConfig(&osc_init);

    clk_init.ClockType  = RCC_CLOCKTYPE_HCLK  | RCC_CLOCKTYPE_SYSCLK
                        | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2
                        | RCC_CLOCKTYPE_D3PCLK1 | RCC_CLOCKTYPE_D1PCLK1;
    clk_init.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk_init.SYSCLKDivider  = RCC_SYSCLK_DIV1;
    clk_init.AHBCLKDivider  = RCC_HCLK_DIV2;       // HCLK = 240 MHz
    clk_init.APB3CLKDivider = RCC_APB3_DIV2;
    clk_init.APB1CLKDivider = RCC_APB1_DIV2;
    clk_init.APB2CLKDivider = RCC_APB2_DIV2;
    clk_init.APB4CLKDivider = RCC_APB4_DIV2;
    HAL_RCC_ClockConfig(&clk_init, FLASH_LATENCY_4);
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

// ---- Rear lighting node message handler ------------------------------------

class RearLightingNodeHandler final
    : public body_control::lighting::transport::TransportMessageHandlerInterface
{
public:
    explicit RearLightingNodeHandler(
        body_control::lighting::platform::stm32::GpioOutputDriver&    gpio,
        body_control::lighting::platform::stm32::Stm32DiagnosticLogger& logger,
        body_control::lighting::transport::TransportAdapterInterface&  transport) noexcept
        : gpio_(gpio)
        , logger_(logger)
        , transport_(transport)
    {
    }

    void OnTransportMessageReceived(
        const body_control::lighting::transport::TransportMessage& msg) override
    {
        using body_control::lighting::transport::SomeipMessageParser;
        using body_control::lighting::domain::LampCommandAction;

        if (!SomeipMessageParser::IsSetLampCommandRequest(msg)) { return; }

        const auto cmd = SomeipMessageParser::ParseLampCommand(msg);
        const bool is_active = (cmd.action == LampCommandAction::kActivate);

        static_cast<void>(gpio_.WriteLampOutput(cmd.function, is_active));
        logger_.LogInfo("Lamp command applied");
    }

    void OnTransportAvailabilityChanged(const bool is_available) override
    {
        if (is_available) { logger_.LogInfo("Transport up"); }
        else              { logger_.LogWarning("Transport down"); }
    }

private:
    body_control::lighting::platform::stm32::GpioOutputDriver&     gpio_;
    body_control::lighting::platform::stm32::Stm32DiagnosticLogger& logger_;
    body_control::lighting::transport::TransportAdapterInterface&   transport_;
};

}  // namespace

// ---- Entry point -----------------------------------------------------------

int main()
{
    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();

    // --- LwIP ---------------------------------------------------------------
    lwip_init();

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
    body_control::lighting::platform::stm32::Stm32DiagnosticLogger  logger {};
    static_cast<void>(logger.Initialize());

    // --- Transport ----------------------------------------------------------
    const body_control::lighting::transport::lwip::LwipUdpConfig udp_config {
        kCzcIpAddr,
        kRearLightingNodePort,
        kCentralZoneControllerPort
    };

    body_control::lighting::transport::lwip::LwipUdpTransportAdapter transport {udp_config};

    RearLightingNodeHandler handler {gpio_driver, logger, transport};
    transport.SetMessageHandler(&handler);

    static_cast<void>(transport.Initialize());

    logger.LogInfo("Rear lighting node started");

    // --- Main loop ----------------------------------------------------------
    std::uint32_t last_tick = HAL_GetTick();

    while (true)
    {
        ethernetif_input(&gnetif);
        sys_check_timeouts();

        const std::uint32_t now     = HAL_GetTick();
        const std::uint32_t delta   = now - last_tick;
        last_tick = now;

        const auto elapsed = std::chrono::milliseconds {delta};

        link_supervisor.UpdateRawLinkState(netif_is_link_up(&gnetif) != 0U);
        link_supervisor.ProcessMainLoop(elapsed);
    }
}

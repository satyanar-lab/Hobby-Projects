#include "body_control/lighting/platform/stm32/stm32_diagnostic_logger.hpp"

#include <cstddef>

#include "stm32h7xx_hal.h"

namespace body_control::lighting::platform::stm32
{

namespace
{

void Transmit(const char* const s, const std::size_t n)
{
    if ((RCC->APB1LENR & RCC_APB1LENR_USART3EN) == 0U) { return; }
    if ((USART3->CR1 & USART_CR1_UE) == 0U) { return; }
    for (std::size_t i = 0U; i < n; ++i)
    {
        while ((USART3->ISR & USART_ISR_TXE_TXFNF) == 0U) {}
        USART3->TDR = static_cast<uint8_t>(s[i]);
    }
    while ((USART3->ISR & USART_ISR_TC) == 0U) {}
}

}  // namespace

bool Stm32DiagnosticLogger::Initialize()
{
    is_initialized_ = ((RCC->APB1LENR & RCC_APB1LENR_USART3EN) != 0U)
                   && ((USART3->CR1 & USART_CR1_UE) != 0U);
    return is_initialized_;
}

void Stm32DiagnosticLogger::Log(
    const DiagnosticLogLevel log_level,
    const std::string_view message)
{
    if (!is_initialized_) { return; }

    const char* prefix = "[   ] ";
    switch (log_level)
    {
        case DiagnosticLogLevel::kDebug:   prefix = "[DBG] "; break;
        case DiagnosticLogLevel::kInfo:    prefix = "[INF] "; break;
        case DiagnosticLogLevel::kWarning: prefix = "[WRN] "; break;
        case DiagnosticLogLevel::kError:   prefix = "[ERR] "; break;
        default: break;
    }

    Transmit(prefix, std::char_traits<char>::length(prefix));
    const std::size_t msg_len = (message.size() < 120U) ? message.size() : 120U;
    Transmit(message.data(), msg_len);
    Transmit("\r\n", 2U);
}

void Stm32DiagnosticLogger::LogDebug(const std::string_view message)
{
    Log(DiagnosticLogLevel::kDebug, message);
}

void Stm32DiagnosticLogger::LogInfo(const std::string_view message)
{
    Log(DiagnosticLogLevel::kInfo, message);
}

void Stm32DiagnosticLogger::LogWarning(const std::string_view message)
{
    Log(DiagnosticLogLevel::kWarning, message);
}

void Stm32DiagnosticLogger::LogError(const std::string_view message)
{
    Log(DiagnosticLogLevel::kError, message);
}

bool Stm32DiagnosticLogger::IsInitialized() const noexcept
{
    return is_initialized_;
}

}  // namespace body_control::lighting::platform::stm32

#ifndef BODY_CONTROL_LIGHTING_PLATFORM_STM32_STM32_DIAGNOSTIC_LOGGER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_STM32_STM32_DIAGNOSTIC_LOGGER_HPP_

#include <cstdint>
#include <string_view>

namespace body_control::lighting::platform::stm32
{

/** Severity levels for the STM32 bare-metal logger. */
enum class DiagnosticLogLevel : std::uint8_t
{
    kDebug = 0U,   ///< Verbose trace output.
    kInfo = 1U,    ///< Normal operational events.
    kWarning = 2U, ///< Unexpected but recoverable condition.
    kError = 3U    ///< Non-recoverable fault.
};

/** Lightweight STM32 diagnostic logger backed by USART3.
 *
 *  Initialize() checks that USART3 is clocked and enabled before setting
 *  is_initialized_.  Log() spin-waits on TXE_TXFNF for each byte then waits
 *  for TC (transmission complete) before returning, ensuring the full message
 *  is in the UART shift register before control is returned to the caller.
 *  Messages are capped at 120 characters to bound the spin-wait time.
 *
 *  The USART backend can be replaced with ITM or SEGGER RTT without changing
 *  any code above the platform layer. */
class Stm32DiagnosticLogger
{
public:
    Stm32DiagnosticLogger() noexcept = default;
    ~Stm32DiagnosticLogger() = default;

    Stm32DiagnosticLogger(const Stm32DiagnosticLogger&) = delete;
    Stm32DiagnosticLogger& operator=(const Stm32DiagnosticLogger&) = delete;
    Stm32DiagnosticLogger(Stm32DiagnosticLogger&&) = delete;
    Stm32DiagnosticLogger& operator=(Stm32DiagnosticLogger&&) = delete;

    /** Returns true if USART3 is clocked and enabled; sets is_initialized_. */
    [[nodiscard]] bool Initialize();

    /** Writes "[PFX] message\r\n" to USART3.  No-op if not initialised. */
    void Log(
        DiagnosticLogLevel log_level,
        std::string_view message);

    void LogDebug(std::string_view message);
    void LogInfo(std::string_view message);
    void LogWarning(std::string_view message);
    void LogError(std::string_view message);

    [[nodiscard]] bool IsInitialized() const noexcept;

private:
    bool is_initialized_ {false};
};

}  // namespace body_control::lighting::platform::stm32

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_STM32_STM32_DIAGNOSTIC_LOGGER_HPP_
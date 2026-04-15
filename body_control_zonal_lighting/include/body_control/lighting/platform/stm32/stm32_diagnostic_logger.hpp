#ifndef BODY_CONTROL_LIGHTING_PLATFORM_STM32_STM32_DIAGNOSTIC_LOGGER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_STM32_STM32_DIAGNOSTIC_LOGGER_HPP_

#include <cstdint>
#include <string_view>

namespace body_control::lighting::platform::stm32
{

/**
 * @brief Supported diagnostic log severity levels for the STM32 target.
 */
enum class DiagnosticLogLevel : std::uint8_t
{
    kDebug = 0U,
    kInfo = 1U,
    kWarning = 2U,
    kError = 3U
};

/**
 * @brief Lightweight STM32 diagnostic logger abstraction.
 *
 * The backend can later be mapped to UART, ITM, SEGGER RTT, or another target
 * logging mechanism without changing higher-level code.
 */
class Stm32DiagnosticLogger
{
public:
    Stm32DiagnosticLogger() noexcept = default;
    ~Stm32DiagnosticLogger() = default;

    Stm32DiagnosticLogger(const Stm32DiagnosticLogger&) = delete;
    Stm32DiagnosticLogger& operator=(const Stm32DiagnosticLogger&) = delete;
    Stm32DiagnosticLogger(Stm32DiagnosticLogger&&) = delete;
    Stm32DiagnosticLogger& operator=(Stm32DiagnosticLogger&&) = delete;

    [[nodiscard]] bool Initialize();

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
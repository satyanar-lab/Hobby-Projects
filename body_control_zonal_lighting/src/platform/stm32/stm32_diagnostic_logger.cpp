#include "body_control/lighting/platform/stm32/stm32_diagnostic_logger.hpp"

namespace body_control::lighting::platform::stm32
{

bool Stm32DiagnosticLogger::Initialize()
{
    is_initialized_ = true;
    return true;
}

void Stm32DiagnosticLogger::Log(
    const DiagnosticLogLevel log_level,
    const std::string_view message)
{
    static_cast<void>(log_level);
    static_cast<void>(message);
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
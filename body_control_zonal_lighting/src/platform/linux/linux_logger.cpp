#include "body_control/lighting/platform/linux/linux_logger.hpp"

#include <iostream>

namespace body_control::lighting::platform::linux
{
namespace
{

[[nodiscard]] const char* ToString(const LogLevel log_level) noexcept
{
    const char* log_level_text {"UNKNOWN"};

    switch (log_level)
    {
    case LogLevel::kDebug:
        log_level_text = "DEBUG";
        break;

    case LogLevel::kInfo:
        log_level_text = "INFO";
        break;

    case LogLevel::kWarning:
        log_level_text = "WARNING";
        break;

    case LogLevel::kError:
        log_level_text = "ERROR";
        break;

    default:
        log_level_text = "UNKNOWN";
        break;
    }

    return log_level_text;
}

}  // namespace

LinuxLogger::LinuxLogger(const std::string& component_name)
    : component_name_(component_name)
{
}

void LinuxLogger::Log(
    const LogLevel log_level,
    const std::string_view message) const
{
    std::ostream& output_stream =
        (log_level == LogLevel::kError) ? std::cerr : std::cout;

    output_stream << "[" << ToString(log_level) << "] "
                  << "[" << component_name_ << "] "
                  << message
                  << '\n';
}

void LinuxLogger::LogDebug(const std::string_view message) const
{
    Log(LogLevel::kDebug, message);
}

void LinuxLogger::LogInfo(const std::string_view message) const
{
    Log(LogLevel::kInfo, message);
}

void LinuxLogger::LogWarning(const std::string_view message) const
{
    Log(LogLevel::kWarning, message);
}

void LinuxLogger::LogError(const std::string_view message) const
{
    Log(LogLevel::kError, message);
}

}  // namespace body_control::lighting::platform::linux
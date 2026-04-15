#ifndef BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_LOGGER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_LOGGER_HPP_

#include <cstdint>
#include <string>
#include <string_view>

namespace body_control::lighting::platform::linux
{

/**
 * @brief Supported logger severity levels.
 */
enum class LogLevel : std::uint8_t
{
    kDebug = 0U,
    kInfo = 1U,
    kWarning = 2U,
    kError = 3U
};

/**
 * @brief Lightweight Linux logger abstraction.
 *
 * The concrete implementation can write to stdout / stderr now and be replaced
 * later by syslog or another automotive-oriented logging backend if needed.
 */
class LinuxLogger
{
public:
    explicit LinuxLogger(const std::string& component_name);

    LinuxLogger(const LinuxLogger&) = default;
    LinuxLogger& operator=(const LinuxLogger&) = default;
    LinuxLogger(LinuxLogger&&) = default;
    LinuxLogger& operator=(LinuxLogger&&) = default;

    ~LinuxLogger() = default;

    void Log(
        LogLevel log_level,
        std::string_view message) const;

    void LogDebug(std::string_view message) const;
    void LogInfo(std::string_view message) const;
    void LogWarning(std::string_view message) const;
    void LogError(std::string_view message) const;

private:
    std::string component_name_ {};
};

}  // namespace body_control::lighting::platform::linux

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_LOGGER_HPP_
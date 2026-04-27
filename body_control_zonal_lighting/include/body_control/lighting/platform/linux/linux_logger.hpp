#ifndef BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_LOGGER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_LOGGER_HPP_

#include <cstdint>
#include <string>
#include <string_view>

namespace body_control::lighting::platform::linux
{

/** Severity levels in ascending order; kError routes to stderr. */
enum class LogLevel : std::uint8_t
{
    kDebug = 0U,   ///< Verbose diagnostic output, disabled in production builds.
    kInfo = 1U,    ///< Normal operational events (init, shutdown, state changes).
    kWarning = 2U, ///< Unexpected but recoverable condition.
    kError = 3U    ///< Non-recoverable fault; written to stderr.
};

/** Lightweight Linux logger abstraction.
 *
 *  Formats messages as "[LEVEL] [component] message" and writes them to
 *  stdout (kDebug/kInfo/kWarning) or stderr (kError).  The backend can be
 *  swapped for syslog or an automotive logging daemon without changing any
 *  higher-level code. */
class LinuxLogger
{
public:
    /** Constructs a logger tagged with component_name in every output line. */
    explicit LinuxLogger(const std::string& component_name);

    LinuxLogger(const LinuxLogger&) = default;
    LinuxLogger& operator=(const LinuxLogger&) = default;
    LinuxLogger(LinuxLogger&&) = default;
    LinuxLogger& operator=(LinuxLogger&&) = default;

    ~LinuxLogger() = default;

    /** Writes message at the given severity level.  kError goes to stderr. */
    void Log(
        LogLevel log_level,
        std::string_view message) const;

    void LogDebug(std::string_view message) const;
    void LogInfo(std::string_view message) const;
    void LogWarning(std::string_view message) const;
    void LogError(std::string_view message) const;

private:
    std::string component_name_ {}; ///< Inserted into every log line for source identification.
};

}  // namespace body_control::lighting::platform::linux

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_LOGGER_HPP_
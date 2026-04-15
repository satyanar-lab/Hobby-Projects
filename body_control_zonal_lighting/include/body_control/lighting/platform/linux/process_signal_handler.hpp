#ifndef BODY_CONTROL_LIGHTING_PLATFORM_LINUX_PROCESS_SIGNAL_HANDLER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_LINUX_PROCESS_SIGNAL_HANDLER_HPP_

#include <atomic>
#include <cstdint>

namespace body_control::lighting::platform::linux
{

/**
 * @brief Result status for signal handler installation.
 */
enum class SignalHandlerStatus : std::uint8_t
{
    kSuccess = 0U,
    kInstallationFailed = 1U
};

/**
 * @brief Process-level signal handler used for graceful shutdown on Linux.
 */
class ProcessSignalHandler
{
public:
    ProcessSignalHandler() noexcept = default;
    ~ProcessSignalHandler() = default;

    ProcessSignalHandler(const ProcessSignalHandler&) = delete;
    ProcessSignalHandler& operator=(const ProcessSignalHandler&) = delete;
    ProcessSignalHandler(ProcessSignalHandler&&) = delete;
    ProcessSignalHandler& operator=(ProcessSignalHandler&&) = delete;

    [[nodiscard]] SignalHandlerStatus Install() noexcept;

    [[nodiscard]] static bool IsShutdownRequested() noexcept;

    [[nodiscard]] static int GetLastSignalNumber() noexcept;

private:
    static void HandleSignal(int signal_number) noexcept;

    static std::atomic<bool> shutdown_requested_;
    static std::atomic<int> last_signal_number_;
};

}  // namespace body_control::lighting::platform::linux

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_LINUX_PROCESS_SIGNAL_HANDLER_HPP_
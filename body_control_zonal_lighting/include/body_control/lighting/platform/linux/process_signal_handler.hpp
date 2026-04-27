#ifndef BODY_CONTROL_LIGHTING_PLATFORM_LINUX_PROCESS_SIGNAL_HANDLER_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_LINUX_PROCESS_SIGNAL_HANDLER_HPP_

#include <atomic>
#include <cstdint>

namespace body_control::lighting::platform::linux
{

/** Return status for Install(). */
enum class SignalHandlerStatus : std::uint8_t
{
    kSuccess = 0U,           ///< SIGINT and SIGTERM both registered successfully.
    kInstallationFailed = 1U ///< std::signal() returned SIG_ERR for one or both signals.
};

/** Process-level POSIX signal handler for graceful shutdown on Linux.
 *
 *  Install() registers HandleSignal for SIGINT and SIGTERM.  The main loop
 *  polls IsShutdownRequested() and initiates Shutdown() when it returns true.
 *  All state is held in atomic statics so HandleSignal() satisfies the
 *  async-signal-safety requirement (no heap allocation, no mutex). */
class ProcessSignalHandler
{
public:
    ProcessSignalHandler() noexcept = default;
    ~ProcessSignalHandler() = default;

    ProcessSignalHandler(const ProcessSignalHandler&) = delete;
    ProcessSignalHandler& operator=(const ProcessSignalHandler&) = delete;
    ProcessSignalHandler(ProcessSignalHandler&&) = delete;
    ProcessSignalHandler& operator=(ProcessSignalHandler&&) = delete;

    /** Registers HandleSignal for SIGINT and SIGTERM.
     *  Resets shutdown_requested_ and last_signal_number_ before registering. */
    [[nodiscard]] SignalHandlerStatus Install() noexcept;

    /** Returns true once HandleSignal has been called; main loop exit trigger. */
    [[nodiscard]] static bool IsShutdownRequested() noexcept;

    /** Returns the signal number that triggered shutdown (0 if not yet signalled). */
    [[nodiscard]] static int GetLastSignalNumber() noexcept;

private:
    /** Async-signal-safe handler: stores signal_number and sets shutdown flag. */
    static void HandleSignal(int signal_number) noexcept;

    static std::atomic<bool> shutdown_requested_; ///< Set true by HandleSignal; polled by main loop.
    static std::atomic<int>  last_signal_number_; ///< Diagnostic: which signal caused the shutdown.
};

}  // namespace body_control::lighting::platform::linux

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_LINUX_PROCESS_SIGNAL_HANDLER_HPP_
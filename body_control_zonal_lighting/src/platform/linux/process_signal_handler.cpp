#include "body_control/lighting/platform/linux/process_signal_handler.hpp"

#include <csignal>

namespace body_control::lighting::platform::linux
{

std::atomic<bool> ProcessSignalHandler::shutdown_requested_ {false};
std::atomic<int> ProcessSignalHandler::last_signal_number_ {0};

SignalHandlerStatus ProcessSignalHandler::Install() noexcept
{
    shutdown_requested_.store(false);
    last_signal_number_.store(0);

    const auto sigint_result =
        std::signal(SIGINT, &ProcessSignalHandler::HandleSignal);
    const auto sigterm_result =
        std::signal(SIGTERM, &ProcessSignalHandler::HandleSignal);

    if ((sigint_result == SIG_ERR) || (sigterm_result == SIG_ERR))
    {
        return SignalHandlerStatus::kInstallationFailed;
    }

    return SignalHandlerStatus::kSuccess;
}

bool ProcessSignalHandler::IsShutdownRequested() noexcept
{
    return shutdown_requested_.load();
}

int ProcessSignalHandler::GetLastSignalNumber() noexcept
{
    return last_signal_number_.load();
}

void ProcessSignalHandler::HandleSignal(const int signal_number) noexcept
{
    last_signal_number_.store(signal_number);
    shutdown_requested_.store(true);
}

}  // namespace body_control::lighting::platform::linux
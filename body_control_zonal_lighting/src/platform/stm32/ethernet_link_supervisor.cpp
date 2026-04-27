#include "body_control/lighting/platform/stm32/ethernet_link_supervisor.hpp"

namespace body_control::lighting::platform::stm32
{

void EthernetLinkSupervisor::Reset() noexcept
{
    raw_link_up_ = false;
    supervised_link_up_ = false;
    link_state_stable_time_ = std::chrono::milliseconds {0};
}

void EthernetLinkSupervisor::UpdateRawLinkState(
    const bool is_link_up) noexcept
{
    if (raw_link_up_ != is_link_up)
    {
        // Transition detected: reset the stable timer so the new state must
        // persist for a full kLinkDebounceTime before being promoted.
        raw_link_up_ = is_link_up;
        link_state_stable_time_ = std::chrono::milliseconds {0};
    }
}

void EthernetLinkSupervisor::ProcessMainLoop(
    const std::chrono::milliseconds elapsed_time) noexcept
{
    if (raw_link_up_ == supervised_link_up_)
    {
        // States are already aligned; nothing to debounce.
        link_state_stable_time_ = std::chrono::milliseconds {0};
        return;
    }

    link_state_stable_time_ += elapsed_time;

    if (link_state_stable_time_ >= kLinkDebounceTime)
    {
        // Raw state has been stable long enough: promote it.
        supervised_link_up_ = raw_link_up_;
        link_state_stable_time_ = std::chrono::milliseconds {0};
    }
}

EthernetLinkState EthernetLinkSupervisor::GetLinkState() const noexcept
{
    return supervised_link_up_ ? EthernetLinkState::kUp
                               : EthernetLinkState::kDown;
}

bool EthernetLinkSupervisor::IsLinkUp() const noexcept
{
    return (GetLinkState() == EthernetLinkState::kUp);
}

}  // namespace body_control::lighting::platform::stm32
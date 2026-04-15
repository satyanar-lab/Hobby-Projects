#ifndef BODY_CONTROL_LIGHTING_PLATFORM_STM32_ETHERNET_LINK_SUPERVISOR_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_STM32_ETHERNET_LINK_SUPERVISOR_HPP_

#include <chrono>
#include <cstdint>

namespace body_control::lighting::platform::stm32
{

/**
 * @brief Supervised Ethernet link state.
 */
enum class EthernetLinkState : std::uint8_t
{
    kDown = 0U,
    kUp = 1U
};

/**
 * @brief Supervises Ethernet PHY link stability on the STM32 target.
 *
 * The intention is to debounce short link glitches before the application
 * reacts to them.
 */
class EthernetLinkSupervisor
{
public:
    EthernetLinkSupervisor() noexcept = default;
    ~EthernetLinkSupervisor() = default;

    EthernetLinkSupervisor(const EthernetLinkSupervisor&) = delete;
    EthernetLinkSupervisor& operator=(const EthernetLinkSupervisor&) = delete;
    EthernetLinkSupervisor(EthernetLinkSupervisor&&) = delete;
    EthernetLinkSupervisor& operator=(EthernetLinkSupervisor&&) = delete;

    void Reset() noexcept;

    void UpdateRawLinkState(
        bool is_link_up) noexcept;

    void ProcessMainLoop(
        std::chrono::milliseconds elapsed_time) noexcept;

    [[nodiscard]] EthernetLinkState GetLinkState() const noexcept;

    [[nodiscard]] bool IsLinkUp() const noexcept;

private:
    static constexpr std::chrono::milliseconds kLinkDebounceTime {100};

    bool raw_link_up_ {false};
    bool supervised_link_up_ {false};
    std::chrono::milliseconds link_state_stable_time_ {0};
};

}  // namespace body_control::lighting::platform::stm32

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_STM32_ETHERNET_LINK_SUPERVISOR_HPP_
#ifndef BODY_CONTROL_LIGHTING_PLATFORM_STM32_ETHERNET_LINK_SUPERVISOR_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_STM32_ETHERNET_LINK_SUPERVISOR_HPP_

#include <chrono>
#include <cstdint>

namespace body_control::lighting::platform::stm32
{

/** Debounced PHY link state exposed to the application. */
enum class EthernetLinkState : std::uint8_t
{
    kDown = 0U, ///< Link absent or not yet stable for kLinkDebounceTime.
    kUp = 1U    ///< Link confirmed stable for at least kLinkDebounceTime.
};

/** Debounces Ethernet PHY link state changes on the STM32 target.
 *
 *  The main loop calls UpdateRawLinkState() with the PHY's instantaneous
 *  link bit, then ProcessMainLoop() with the loop's elapsed time.  The
 *  supervised state only transitions after the raw state has been stable for
 *  kLinkDebounceTime, suppressing transient glitches that would otherwise
 *  trigger unnecessary LwIP link-down/up cycles. */
class EthernetLinkSupervisor
{
public:
    EthernetLinkSupervisor() noexcept = default;
    ~EthernetLinkSupervisor() = default;

    EthernetLinkSupervisor(const EthernetLinkSupervisor&) = delete;
    EthernetLinkSupervisor& operator=(const EthernetLinkSupervisor&) = delete;
    EthernetLinkSupervisor(EthernetLinkSupervisor&&) = delete;
    EthernetLinkSupervisor& operator=(EthernetLinkSupervisor&&) = delete;

    /** Resets both raw and supervised state to link-down; clears stable timer. */
    void Reset() noexcept;

    /** Feeds a new raw PHY sample; resets the stable timer on any transition. */
    void UpdateRawLinkState(
        bool is_link_up) noexcept;

    /** Accumulates elapsed_time toward the debounce threshold and promotes the
     *  supervised state when the raw state has been stable long enough. */
    void ProcessMainLoop(
        std::chrono::milliseconds elapsed_time) noexcept;

    /** Returns the debounced link state. */
    [[nodiscard]] EthernetLinkState GetLinkState() const noexcept;

    /** Convenience wrapper: true when GetLinkState() == kUp. */
    [[nodiscard]] bool IsLinkUp() const noexcept;

private:
    // 100 ms suppresses cable-unplug bounce on standard 100BASE-TX PHYs.
    static constexpr std::chrono::milliseconds kLinkDebounceTime {100};

    bool raw_link_up_ {false};          ///< Latest PHY sample from UpdateRawLinkState().
    bool supervised_link_up_ {false};   ///< Debounced state reported to the application.
    std::chrono::milliseconds link_state_stable_time_ {0}; ///< Time raw state has been unchanged.
};

}  // namespace body_control::lighting::platform::stm32

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_STM32_ETHERNET_LINK_SUPERVISOR_HPP_
#ifndef BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_CLOCK_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_CLOCK_HPP_

#include <chrono>
#include <cstdint>

namespace body_control::lighting::platform::linux
{

/**
 * @brief Linux clock abstraction based on std::chrono::steady_clock.
 *
 * This wrapper keeps timing access centralized so the application layer does
 * not directly depend on clock selection details.
 */
class LinuxClock
{
public:
    using TimePoint = std::chrono::steady_clock::time_point;
    using Milliseconds = std::chrono::milliseconds;

    LinuxClock() noexcept = default;
    ~LinuxClock() = default;

    LinuxClock(const LinuxClock&) = default;
    LinuxClock& operator=(const LinuxClock&) = default;
    LinuxClock(LinuxClock&&) = default;
    LinuxClock& operator=(LinuxClock&&) = default;

    [[nodiscard]] TimePoint GetCurrentTime() const noexcept;

    [[nodiscard]] std::uint64_t GetCurrentTimeMilliseconds() const noexcept;

    [[nodiscard]] static Milliseconds GetElapsedTime(
        const TimePoint& start_time,
        const TimePoint& end_time) noexcept;
};

}  // namespace body_control::lighting::platform::linux

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_CLOCK_HPP_
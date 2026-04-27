#ifndef BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_CLOCK_HPP_
#define BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_CLOCK_HPP_

#include <chrono>
#include <cstdint>

namespace body_control::lighting::platform::linux
{

/** Linux clock abstraction based on std::chrono::steady_clock.
 *
 *  steady_clock is monotonic: it never goes backwards and is unaffected by
 *  wall-clock adjustments (NTP slew, DST).  This makes it safe for timeout
 *  and heartbeat calculations in the health monitor. */
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

    /** Returns the current steady-clock time point. */
    [[nodiscard]] TimePoint GetCurrentTime() const noexcept;

    /** Returns milliseconds elapsed since the steady-clock epoch as a uint64.
     *  Useful when a plain integer timestamp is needed (e.g. logging). */
    [[nodiscard]] std::uint64_t GetCurrentTimeMilliseconds() const noexcept;

    /** Returns the duration between start_time and end_time in milliseconds.
     *  Caller is responsible for ensuring end_time >= start_time. */
    [[nodiscard]] static Milliseconds GetElapsedTime(
        const TimePoint& start_time,
        const TimePoint& end_time) noexcept;
};

}  // namespace body_control::lighting::platform::linux

#endif  // BODY_CONTROL_LIGHTING_PLATFORM_LINUX_LINUX_CLOCK_HPP_
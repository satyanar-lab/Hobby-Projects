#include "body_control/lighting/platform/linux/linux_clock.hpp"

namespace body_control::lighting::platform::linux
{

LinuxClock::TimePoint LinuxClock::GetCurrentTime() const noexcept
{
    return std::chrono::steady_clock::now();
}

std::uint64_t LinuxClock::GetCurrentTimeMilliseconds() const noexcept
{
    const auto now_time = GetCurrentTime();
    // time_since_epoch() on steady_clock is unspecified in absolute terms but
    // increases monotonically; casting to milliseconds gives a stable counter
    // suitable for log timestamps and timeout deltas.
    const auto now_milliseconds =
        std::chrono::duration_cast<Milliseconds>(now_time.time_since_epoch());

    return static_cast<std::uint64_t>(now_milliseconds.count());
}

LinuxClock::Milliseconds LinuxClock::GetElapsedTime(
    const TimePoint& start_time,
    const TimePoint& end_time) noexcept
{
    return std::chrono::duration_cast<Milliseconds>(end_time - start_time);
}

}  // namespace body_control::lighting::platform::linux
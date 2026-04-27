#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_CONSTANTS_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_CONSTANTS_HPP_

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace body_control::lighting::domain
{

/** Sequence counter range and sentinel values for LampCommand. */
namespace sequence_counter
{

/// Zero is never issued as a valid counter so a zero-init or reset message
/// is caught immediately without a separate validity flag.
constexpr std::uint16_t kInvalidValue {0U};

/// First counter value the sequencer issues after startup or wrap-around.
constexpr std::uint16_t kFirstValidValue {1U};

/// uint16_t ceiling; the counter wraps to kFirstValidValue on the next issue.
constexpr std::uint16_t kMaximumValue {65535U};

}  // namespace sequence_counter

/** Timing constants for blink patterns, publish rates, and watchdog intervals. */
namespace timing
{

/// Indicator ON half-period — 500 ms gives a 1 Hz flash rate matching
/// ISO 3731 and UNECE Regulation 6 for direction indicators.
constexpr std::chrono::milliseconds kIndicatorOnTime {500};

/// Indicator OFF half-period — symmetric with ON to produce a 50 % duty cycle.
constexpr std::chrono::milliseconds kIndicatorOffTime {500};

/// Hazard ON half-period — same rate as the indicator per UNECE R6 §6.
constexpr std::chrono::milliseconds kHazardOnTime {500};

/// Hazard OFF half-period — symmetric with ON.
constexpr std::chrono::milliseconds kHazardOffTime {500};

/// How often the rear node publishes per-lamp output state to the controller.
/// 100 ms (10 Hz) is fast enough for real-time HMI updates with low bus load.
constexpr std::chrono::milliseconds kLampStatusPublishPeriod {100};

/// How often the rear node publishes its aggregated health snapshot.
/// 1 Hz is sufficient for operator display; health changes are not safety-critical.
constexpr std::chrono::milliseconds kNodeHealthPublishPeriod {1000};

/// If no health publish arrives within this window the controller declares the
/// node kUnavailable.  2 000 ms = 2× the publish period, tolerating one missed
/// packet before escalating.
constexpr std::chrono::milliseconds kNodeHeartbeatTimeout {2000};

/// Main control-loop tick period.  20 ms (50 Hz) keeps blink timing accurate
/// to within one tick (±20 ms) without burning CPU on a tighter poll rate.
constexpr std::chrono::milliseconds kMainLoopPeriod {20};

}  // namespace timing

/** Limits for the diagnostic fault table. */
namespace diagnostics
{

/// Maximum number of simultaneously active faults the node tracks.
/// 32 entries covers all lamp channels with room for system-level faults.
constexpr std::uint16_t kMaximumActiveFaultCount {32U};

}  // namespace diagnostics

/** Compile-time capacity limits for internal data structures. */
namespace system_limits
{

/// Number of distinct lamp functions in the system (Left, Right, Hazard, Park, Head).
/// Used to size per-function arrays without magic numbers.
constexpr std::size_t kMaximumLampFunctionCount {5U};

/// Depth of the arbitrator's command history ring buffer.
/// 16 entries is enough to trace the last several operator interactions for debugging.
constexpr std::size_t kMaximumCommandHistoryDepth {16U};

}  // namespace system_limits

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_CONSTANTS_HPP_

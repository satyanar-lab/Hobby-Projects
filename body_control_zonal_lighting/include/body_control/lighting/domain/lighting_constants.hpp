#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_CONSTANTS_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_CONSTANTS_HPP_

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace body_control::lighting::domain
{

namespace sequence_counter
{

constexpr std::uint16_t kInvalidValue {0U};
constexpr std::uint16_t kFirstValidValue {1U};
constexpr std::uint16_t kMaximumValue {65535U};

}  // namespace sequence_counter

namespace timing
{

constexpr std::chrono::milliseconds kIndicatorOnTime {500};
constexpr std::chrono::milliseconds kIndicatorOffTime {500};
constexpr std::chrono::milliseconds kHazardOnTime {500};
constexpr std::chrono::milliseconds kHazardOffTime {500};

constexpr std::chrono::milliseconds kLampStatusPublishPeriod {100};
constexpr std::chrono::milliseconds kNodeHealthPublishPeriod {1000};
constexpr std::chrono::milliseconds kNodeHeartbeatTimeout {2000};
constexpr std::chrono::milliseconds kMainLoopPeriod {20};

}  // namespace timing

namespace diagnostics
{

constexpr std::uint16_t kMaximumActiveFaultCount {32U};

}  // namespace diagnostics

namespace system_limits
{

constexpr std::size_t kMaximumLampFunctionCount {5U};
constexpr std::size_t kMaximumCommandHistoryDepth {16U};

}  // namespace system_limits

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_CONSTANTS_HPP_
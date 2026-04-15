#ifndef BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP_
#define BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP_

#include <cstdint>

namespace body_control::lighting::domain
{

/**
 * @brief Rear lighting control service identifiers.
 *
 * This service is used by the Central Zone Controller to command and observe
 * the Rear Lighting Node over Ethernet / SOME-IP style communication.
 */
namespace rear_lighting_service
{

constexpr std::uint16_t kServiceId {0x5100U};
constexpr std::uint16_t kInstanceId {0x0001U};

constexpr std::uint8_t kMajorVersion {1U};
constexpr std::uint32_t kMinorVersion {0U};

/**
 * @brief Request / response method identifiers.
 *
 * Method identifiers are used for direct request-response calls.
 */
constexpr std::uint16_t kSetLampCommandMethodId {0x0001U};
constexpr std::uint16_t kGetLampStatusMethodId  {0x0002U};
constexpr std::uint16_t kGetNodeHealthMethodId  {0x0003U};
constexpr std::uint16_t kResetNodeFaultsMethodId {0x0004U};

/**
 * @brief Event identifiers.
 *
 * Event identifiers are used for publish / subscribe style notifications.
 * The 0x8xxx range is intentionally used to keep event identifiers visually
 * distinct from request / response method identifiers.
 */
constexpr std::uint16_t kLampStatusEventId {0x8001U};
constexpr std::uint16_t kNodeHealthEventId {0x8002U};

/**
 * @brief Event group identifiers.
 *
 * Event groups are used by subscribers to request a group of events.
 */
constexpr std::uint16_t kLampStatusEventGroupId {0x0001U};
constexpr std::uint16_t kNodeHealthEventGroupId {0x0002U};

constexpr bool IsMethodId(const std::uint16_t method_id) noexcept
{
    return (method_id == kSetLampCommandMethodId) ||
           (method_id == kGetLampStatusMethodId)  ||
           (method_id == kGetNodeHealthMethodId)  ||
           (method_id == kResetNodeFaultsMethodId);
}

constexpr bool IsEventId(const std::uint16_t event_id) noexcept
{
    return (event_id == kLampStatusEventId) ||
           (event_id == kNodeHealthEventId);
}

}  // namespace rear_lighting_service

/**
 * @brief Diagnostic support service identifiers.
 *
 * This service is intentionally separate from the rear lighting control
 * service so that control traffic and diagnostic traffic are kept logically
 * decoupled.
 */
namespace diagnostic_service
{

constexpr std::uint16_t kServiceId {0x5101U};
constexpr std::uint16_t kInstanceId {0x0001U};

constexpr std::uint8_t kMajorVersion {1U};
constexpr std::uint32_t kMinorVersion {0U};

constexpr std::uint16_t kGetDiagnosticSnapshotMethodId {0x0001U};
constexpr std::uint16_t kClearDiagnosticSnapshotMethodId {0x0002U};

constexpr std::uint16_t kDiagnosticSnapshotEventId {0x8001U};
constexpr std::uint16_t kDiagnosticSnapshotEventGroupId {0x0001U};

constexpr bool IsMethodId(const std::uint16_t method_id) noexcept
{
    return (method_id == kGetDiagnosticSnapshotMethodId) ||
           (method_id == kClearDiagnosticSnapshotMethodId);
}

constexpr bool IsEventId(const std::uint16_t event_id) noexcept
{
    return (event_id == kDiagnosticSnapshotEventId);
}

}  // namespace diagnostic_service

}  // namespace body_control::lighting::domain

#endif  // BODY_CONTROL_LIGHTING_DOMAIN_LIGHTING_SERVICE_IDS_HPP_
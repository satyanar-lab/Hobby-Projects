#ifndef BODY_CONTROL_LIGHTING_SERVICE_DIAGNOSTIC_SERVICE_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_DIAGNOSTIC_SERVICE_INTERFACE_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::service
{

/**
 * @brief Result status for diagnostic service operations.
 */
enum class DiagnosticServiceStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kNotAvailable = 2U,
    kInvalidArgument = 3U,
    kTransportFailure = 4U
};

/**
 * @brief Lightweight diagnostic snapshot for the lighting node.
 */
struct DiagnosticSnapshot
{
    domain::NodeHealthStatus node_health_status {};
    std::uint16_t last_processed_sequence_counter {0U};
};

/**
 * @brief Application-facing diagnostic service abstraction.
 */
class DiagnosticServiceInterface
{
public:
    virtual ~DiagnosticServiceInterface() = default;

    [[nodiscard]] virtual DiagnosticServiceStatus Initialize() = 0;
    [[nodiscard]] virtual DiagnosticServiceStatus Shutdown() = 0;

    [[nodiscard]] virtual DiagnosticServiceStatus RequestDiagnosticSnapshot() = 0;

    [[nodiscard]] virtual bool IsServiceAvailable() const noexcept = 0;
};

}  // namespace body_control::lighting::service

#endif  // BODY_CONTROL_LIGHTING_SERVICE_DIAGNOSTIC_SERVICE_INTERFACE_HPP_
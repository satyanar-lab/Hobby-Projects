#ifndef BODY_CONTROL_LIGHTING_SERVICE_DIAGNOSTIC_SERVICE_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_DIAGNOSTIC_SERVICE_INTERFACE_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control::lighting::service
{

/**
 * Return status for diagnostic service operations.
 */
enum class DiagnosticServiceStatus : std::uint8_t
{
    kSuccess         = 0U,  ///< Operation completed without error.
    kNotInitialized  = 1U,  ///< Initialize() has not been called.
    kNotAvailable    = 2U,  ///< The target node is not reachable.
    kInvalidArgument = 3U,  ///< A parameter is outside its valid range.
    kTransportFailure = 4U  ///< The transport layer failed to send or receive.
};

/**
 * Lightweight snapshot of node diagnostic data returned by a diagnostic query.
 *
 * Intended for the engineering diagnostic console tool, which displays
 * node health and sequence tracking without going through the full HMI stack.
 */
struct DiagnosticSnapshot
{
    /// Aggregated health state at the time the snapshot was taken.
    domain::NodeHealthStatus node_health_status {};

    /**
     * Sequence counter of the most recently processed LampCommand on the node.
     * The diagnostic console uses this to verify that commands sent from the
     * HMI side are being received and applied in order.
     */
    std::uint16_t last_processed_sequence_counter {0U};
};

/**
 * Abstract interface for accessing node diagnostic data.
 *
 * Implemented by the diagnostic console's service consumer.  Provides a
 * narrower surface than the full operator service — only read operations,
 * no lamp commands — suitable for engineering use during bring-up and testing.
 */
class DiagnosticServiceInterface
{
public:
    virtual ~DiagnosticServiceInterface() = default;

    /**
     * Connects to the transport and prepares to issue diagnostic requests.
     *
     * @return kSuccess or kTransportFailure.
     */
    [[nodiscard]] virtual DiagnosticServiceStatus Initialize() = 0;

    /**
     * Disconnects from the transport and releases all resources.
     *
     * @return kSuccess; never fails.
     */
    [[nodiscard]] virtual DiagnosticServiceStatus Shutdown() = 0;

    /**
     * Requests a fresh DiagnosticSnapshot from the node.
     *
     * The result is delivered asynchronously; implementations typically
     * update an internal cache that the caller then reads.
     *
     * @return kSuccess if the request was sent, or an error status.
     */
    [[nodiscard]] virtual DiagnosticServiceStatus RequestDiagnosticSnapshot() = 0;

    /** Returns true when the diagnostic service endpoint is reachable. */
    [[nodiscard]] virtual bool IsServiceAvailable() const noexcept = 0;
};

}  // namespace body_control::lighting::service

#endif  // BODY_CONTROL_LIGHTING_SERVICE_DIAGNOSTIC_SERVICE_INTERFACE_HPP_

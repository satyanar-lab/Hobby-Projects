#ifndef BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_INTERFACE_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_INTERFACE_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"

namespace body_control
{
namespace lighting
{
namespace service
{

/**
 * @brief Result status for operator service operations.
 *
 * kRejected indicates the request was structurally valid but was blocked
 * by the controller's command arbitration policy (e.g. indicator activate
 * while hazard is active).
 */
enum class OperatorServiceStatus : std::uint8_t
{
    kSuccess = 0U,
    kNotInitialized = 1U,
    kNotAvailable = 2U,
    kInvalidArgument = 3U,
    kRejected = 4U,
    kTransportError = 5U
};

/**
 * @brief Callback interface delivered to HMI / diagnostic console clients.
 *
 * The operator service provider fires these callbacks whenever the central
 * zone controller's state changes, allowing thin clients to stay in sync
 * without polling.
 */
class OperatorServiceEventListenerInterface
{
public:
    virtual ~OperatorServiceEventListenerInterface() = default;

    virtual void OnLampStatusUpdated(
        const domain::LampStatus& lamp_status) = 0;

    virtual void OnNodeHealthUpdated(
        const domain::NodeHealthStatus& node_health_status) = 0;

    virtual void OnControllerAvailabilityChanged(
        bool is_available) = 0;
};

/**
 * @brief Abstract interface for the operator service.
 *
 * Implemented by OperatorServiceProvider (in-process, used by the
 * controller app) and by OperatorServiceConsumer (out-of-process proxy,
 * used by HMI and diagnostic console apps).  Callers hold a pointer to
 * this interface, so the binding — in-process or across UDP — is an
 * implementation detail.
 */
class OperatorServiceProviderInterface
{
public:
    virtual ~OperatorServiceProviderInterface() = default;

    [[nodiscard]] virtual OperatorServiceStatus Initialize() = 0;
    [[nodiscard]] virtual OperatorServiceStatus Shutdown() = 0;

    /**
     * @brief Toggle the output state of the named lamp function.
     *
     * The controller applies its arbitration policy; the request may be
     * rejected (e.g. indicator while hazard is active).
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestLampToggle(
        domain::LampFunction lamp_function) = 0;

    /**
     * @brief Explicitly activate the named lamp function.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestLampActivate(
        domain::LampFunction lamp_function) = 0;

    /**
     * @brief Explicitly deactivate the named lamp function.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestLampDeactivate(
        domain::LampFunction lamp_function) = 0;

    /**
     * @brief Request a fresh node health snapshot from the controller.
     */
    [[nodiscard]] virtual OperatorServiceStatus RequestNodeHealth() = 0;

    /**
     * @brief Read the most recent lamp status for a given function.
     *
     * @return true if lamp_status was populated, false if function unknown.
     */
    [[nodiscard]] virtual bool GetLampStatus(
        domain::LampFunction lamp_function,
        domain::LampStatus& lamp_status) const noexcept = 0;

    /**
     * @brief Read the most recent node health snapshot.
     */
    virtual void GetNodeHealthStatus(
        domain::NodeHealthStatus& node_health_status) const noexcept = 0;

    virtual void SetEventListener(
        OperatorServiceEventListenerInterface* listener) noexcept = 0;
};

}  // namespace service
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_SERVICE_OPERATOR_SERVICE_INTERFACE_HPP_

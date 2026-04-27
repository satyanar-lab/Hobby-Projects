#ifndef BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP
#define BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP

#include "body_control/lighting/hmi/hmi_command_mapper.hpp"
#include "body_control/lighting/hmi/hmi_view_model.hpp"
#include "body_control/lighting/service/operator_service_interface.hpp"

namespace body_control
{
namespace lighting
{
namespace hmi
{

/** Return code from ProcessAction indicating whether the operator service
 *  accepted, rejected, or failed to route the requested lamp command. */
enum class MainWindowStatus : std::uint8_t
{
    kSuccess = 0U,        ///< Command accepted and forwarded to the controller.
    kInvalidAction = 1U,  ///< HmiAction was kUnknown or unrecognised.
    kControllerError = 2U ///< Transport error, service unavailable, or rejected by arbitration.
};

/** Top-level HMI component that bridges user input to the operator service.
 *
 *  Receives HmiAction values from the command mapper (keyboard loop) and
 *  dispatches them as toggle/health requests via OperatorServiceProviderInterface.
 *  Implements OperatorServiceEventListenerInterface so it receives lamp status
 *  and node health events pushed by the consumer, keeping the view model
 *  current without polling. */
class MainWindow final
    : public service::OperatorServiceEventListenerInterface
{
public:
    explicit MainWindow(
        service::OperatorServiceProviderInterface& operator_service) noexcept;

    /** Translates action into a service call and returns whether it succeeded. */
    MainWindowStatus ProcessAction(
        HmiAction action);

    // OperatorServiceEventListenerInterface — called by the consumer on
    // event arrival; keeps the view model up-to-date without polling.
    void OnLampStatusUpdated(
        const domain::LampStatus& lamp_status) override;

    void OnNodeHealthUpdated(
        const domain::NodeHealthStatus& node_health_status) override;

    void OnControllerAvailabilityChanged(
        bool is_available) override;

    /** Provides read-only access to the cached view state for rendering. */
    const HmiViewModel& GetViewModel() const noexcept;

private:
    /** Maps OperatorServiceStatus to the coarser MainWindowStatus vocabulary. */
    static MainWindowStatus ConvertOperatorStatus(
        service::OperatorServiceStatus operator_status) noexcept;

    service::OperatorServiceProviderInterface& operator_service_; ///< Consumer that sends requests to the CZC.
    HmiViewModel hmi_view_model_; ///< Local cache updated by incoming events.
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP

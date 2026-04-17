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

enum class MainWindowStatus : std::uint8_t
{
    kSuccess = 0U,
    kInvalidAction = 1U,
    kControllerError = 2U
};

class MainWindow final
    : public service::OperatorServiceEventListenerInterface
{
public:
    explicit MainWindow(
        service::OperatorServiceProviderInterface& operator_service) noexcept;

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

    const HmiViewModel& GetViewModel() const noexcept;

private:
    static MainWindowStatus ConvertOperatorStatus(
        service::OperatorServiceStatus operator_status) noexcept;

    service::OperatorServiceProviderInterface& operator_service_;
    HmiViewModel hmi_view_model_;
};

}  // namespace hmi
}  // namespace lighting
}  // namespace body_control

#endif  // BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP

#ifndef BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP_
#define BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP_

#include <cstdint>

#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/hmi/hmi_command_mapper.hpp"
#include "body_control/lighting/hmi/hmi_view_model.hpp"

namespace body_control::lighting::application
{

class CentralZoneController;

}  // namespace body_control::lighting::application

namespace body_control::lighting::hmi
{

/**
 * @brief Result status for main window operations.
 */
enum class MainWindowStatus : std::uint8_t
{
    kSuccess = 0U,
    kInvalidAction = 1U,
    kControllerRequestFailed = 2U
};

/**
 * @brief Top-level HMI window coordinator.
 *
 * This class is intentionally kept independent from a specific GUI toolkit.
 * It owns the HMI command mapper and view model, and delegates control actions
 * to the Central Zone Controller.
 */
class MainWindow
{
public:
    explicit MainWindow(
        application::CentralZoneController& central_zone_controller) noexcept;

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;
    MainWindow(MainWindow&&) = delete;
    MainWindow& operator=(MainWindow&&) = delete;

    ~MainWindow() = default;

    [[nodiscard]] MainWindowStatus ProcessAction(
        HmiAction action);

    void UpdateLampStatus(
        const domain::LampStatus& lamp_status) noexcept;

    void UpdateNodeHealthStatus(
        const domain::NodeHealthStatus& node_health_status) noexcept;

    [[nodiscard]] const HmiViewModel& GetViewModel() const noexcept;

private:
    application::CentralZoneController& central_zone_controller_;
    HmiCommandMapper hmi_command_mapper_ {};
    HmiViewModel hmi_view_model_ {};
};

}  // namespace body_control::lighting::hmi

#endif  // BODY_CONTROL_LIGHTING_HMI_MAIN_WINDOW_HPP_
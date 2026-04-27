/**
 * @file test_controller_arbitration_via_operator.cpp
 * @brief Arbitration enforcement through the full operator-service chain.
 *
 * Uses the same wiring as test_operator_service_roundtrip:
 *
 *   OperatorServiceConsumer
 *     ↕ loopback pair A
 *   OperatorServiceProvider → CentralZoneController → RearLightingServiceConsumer
 *     ↕ loopback pair B
 *   RearLightingServiceProvider → RearLightingFunctionManager
 *
 * Verifies that after the hazard lamp is activated, a subsequent toggle
 * request for the left indicator is blocked by the CZC arbitrator and never
 * reaches the rear node, so no OnLampStatusUpdated callback fires for it.
 */

#include <cstdint>
#include <mutex>

#include <gtest/gtest.h>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/service/operator_service_consumer.hpp"
#include "body_control/lighting/service/operator_service_interface.hpp"
#include "body_control/lighting/service/operator_service_provider.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/service/rear_lighting_service_provider.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

namespace
{

using body_control::lighting::application::CentralZoneController;
using body_control::lighting::application::ControllerStatus;
using body_control::lighting::application::RearLightingFunctionManager;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::domain::operator_service::kHmiControlPanelApplicationId;
using body_control::lighting::service::OperatorServiceConsumer;
using body_control::lighting::service::OperatorServiceEventListenerInterface;
using body_control::lighting::service::OperatorServiceProvider;
using body_control::lighting::service::OperatorServiceStatus;
using body_control::lighting::service::RearLightingServiceConsumer;
using body_control::lighting::service::RearLightingServiceProvider;
using body_control::lighting::service::ServiceStatus;
using body_control::lighting::transport::TransportAdapterInterface;
using body_control::lighting::transport::TransportMessage;
using body_control::lighting::transport::TransportMessageHandlerInterface;
using body_control::lighting::transport::TransportStatus;

class LoopbackTransportAdapter final : public TransportAdapterInterface
{
public:
    LoopbackTransportAdapter() = default;

    void SetPeer(LoopbackTransportAdapter* peer) noexcept { peer_ = peer; }

    TransportStatus Initialize() override
    {
        TransportMessageHandlerInterface* handler {nullptr};
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            is_initialized_ = true;
            handler = handler_;
        }
        if (handler != nullptr)
        {
            handler->OnTransportAvailabilityChanged(true);
        }
        return TransportStatus::kSuccess;
    }

    TransportStatus Shutdown() override
    {
        TransportMessageHandlerInterface* handler {nullptr};
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            is_initialized_ = false;
            handler = handler_;
        }
        if (handler != nullptr)
        {
            handler->OnTransportAvailabilityChanged(false);
        }
        return TransportStatus::kSuccess;
    }

    TransportStatus SendRequest(const TransportMessage& m) override { return Forward(m); }
    TransportStatus SendResponse(const TransportMessage& m) override { return Forward(m); }
    TransportStatus SendEvent(const TransportMessage& m) override { return Forward(m); }

    void SetMessageHandler(
        TransportMessageHandlerInterface* handler) noexcept override
    {
        bool replay {false};
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            handler_ = handler;
            replay = (handler_ != nullptr) && is_initialized_;
        }
        if (replay)
        {
            handler->OnTransportAvailabilityChanged(true);
        }
    }

private:
    TransportStatus Forward(const TransportMessage& transport_message)
    {
        TransportMessageHandlerInterface* peer_handler {nullptr};
        {
            const std::lock_guard<std::mutex> lock(mutex_);
            if (!is_initialized_)
            {
                return TransportStatus::kNotInitialized;
            }
            if (peer_ == nullptr)
            {
                return TransportStatus::kServiceUnavailable;
            }
            peer_handler = peer_->handler_;
        }
        if (peer_handler == nullptr)
        {
            return TransportStatus::kServiceUnavailable;
        }
        peer_handler->OnTransportMessageReceived(transport_message);
        return TransportStatus::kSuccess;
    }

    std::mutex mutex_ {};
    TransportMessageHandlerInterface* handler_ {nullptr};
    LoopbackTransportAdapter* peer_ {nullptr};
    bool is_initialized_ {false};
};

// Records only lamp status updates; health and availability events are not
// under test here and are silently consumed.
class RecordingEventListener final : public OperatorServiceEventListenerInterface
{
public:
    void OnLampStatusUpdated(const LampStatus& lamp_status) override
    {
        last_lamp_status_ = lamp_status;
        lamp_status_update_count_++;
    }

    void OnNodeHealthUpdated(const NodeHealthStatus&) override {}
    void OnControllerAvailabilityChanged(bool) override {}

    LampStatus last_lamp_status_ {};
    std::uint32_t lamp_status_update_count_ {0U};
};

}  // namespace

TEST(ControllerArbitrationViaOperator, IndicatorBlockedByActiveHazard)
{
    LoopbackTransportAdapter op_consumer_transport {};
    LoopbackTransportAdapter op_provider_transport {};
    op_consumer_transport.SetPeer(&op_provider_transport);
    op_provider_transport.SetPeer(&op_consumer_transport);

    LoopbackTransportAdapter rear_consumer_transport {};
    LoopbackTransportAdapter rear_provider_transport {};
    rear_consumer_transport.SetPeer(&rear_provider_transport);
    rear_provider_transport.SetPeer(&rear_consumer_transport);

    RearLightingFunctionManager rear_fn_mgr {};
    RearLightingServiceProvider rear_provider {rear_fn_mgr, rear_provider_transport};
    RearLightingServiceConsumer rear_consumer {rear_consumer_transport};
    CentralZoneController controller {rear_consumer};
    OperatorServiceProvider op_provider {controller, op_provider_transport};
    OperatorServiceConsumer op_consumer {op_consumer_transport, kHmiControlPanelApplicationId};

    RecordingEventListener listener {};
    op_consumer.SetEventListener(&listener);

    ASSERT_EQ(rear_provider.Initialize(), ServiceStatus::kSuccess);
    ASSERT_EQ(op_provider.Initialize(), OperatorServiceStatus::kSuccess);
    ASSERT_EQ(controller.Initialize(), ControllerStatus::kSuccess);
    ASSERT_EQ(op_consumer.Initialize(), OperatorServiceStatus::kSuccess);

    // First toggle: hazard lamp is off → activates.  The arbitrator expands
    // hazard activate into three commands (hazard, left indicator, right
    // indicator), so three LampStatus events flow back synchronously.
    // After these three events the CZC lamp-state manager records hazard,
    // left indicator, and right indicator all as active.
    EXPECT_EQ(
        op_consumer.RequestLampToggle(LampFunction::kHazardLamp),
        OperatorServiceStatus::kSuccess);

    ASSERT_EQ(listener.lamp_status_update_count_, 3U);
    EXPECT_EQ(listener.last_lamp_status_.function,     LampFunction::kRightIndicator);
    EXPECT_EQ(listener.last_lamp_status_.output_state, LampOutputState::kOn);

    // Second toggle: left indicator while hazard is active.  The CZC
    // arbitrator rejects the command; no message reaches the rear node,
    // so no LampStatus event is published and the listener count stays at 3.
    EXPECT_EQ(
        op_consumer.RequestLampToggle(LampFunction::kLeftIndicator),
        OperatorServiceStatus::kSuccess);

    EXPECT_EQ(listener.lamp_status_update_count_, 3U);
    EXPECT_EQ(listener.last_lamp_status_.function, LampFunction::kRightIndicator);

    EXPECT_EQ(op_consumer.Shutdown(), OperatorServiceStatus::kSuccess);
    EXPECT_EQ(controller.Shutdown(), ControllerStatus::kSuccess);
    EXPECT_EQ(op_provider.Shutdown(), OperatorServiceStatus::kSuccess);
    EXPECT_EQ(rear_provider.Shutdown(), ServiceStatus::kSuccess);
}

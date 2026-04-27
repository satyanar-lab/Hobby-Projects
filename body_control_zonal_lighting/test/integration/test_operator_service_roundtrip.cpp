/**
 * @file test_operator_service_roundtrip.cpp
 * @brief Full operator-service round-trip integration test.
 *
 * Wires the complete chain:
 *
 *   OperatorServiceConsumer (HMI side)
 *     ↕ op_consumer_transport / op_provider_transport  (loopback pair A)
 *   OperatorServiceProvider  (controller side)
 *     ↕ CentralZoneController
 *     ↕ RearLightingServiceConsumer
 *     ↕ rear_consumer_transport / rear_provider_transport  (loopback pair B)
 *   RearLightingServiceProvider
 *     ↕ RearLightingFunctionManager
 *
 * All transports are synchronous loopbacks: every Send* call delivers the
 * message to the peer's handler inline on the same call stack before
 * returning, so assertions are reliable without sleeps or polling.
 *
 * CentralZoneController::Initialize() starts a background health-poll
 * thread (period = kNodeHealthPublishPeriod = 1 000 ms).  The test
 * completes in well under 10 ms, so concurrent transport access from that
 * thread does not occur in practice.  The loopback adapter captures the
 * peer's handler pointer under its own mutex before releasing it, making
 * any hypothetical concurrent Forward() calls data-race free.
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

/**
 * @brief Synchronous in-process transport adapter that forwards messages to a peer.
 *
 * Every Send* call delivers the message to the peer's handler on the same
 * call stack.  The handler pointer is captured under the adapter's own mutex
 * before the call is dispatched, so concurrent calls from the CZC health
 * poll thread are data-race free.
 */
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
            // Snapshot the peer's handler pointer under our own lock.
            // The peer's handler_ is written only during init/shutdown, not
            // concurrently with Forward(), so the unsynchronised read here is
            // safe for this loopback-only test helper.
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

TEST(OperatorServiceRoundtrip, ParkLampToggleDeliveredToConsumer)
{
    // Operator transport pair: HMI consumer ↔ controller-side provider.
    LoopbackTransportAdapter op_consumer_transport {};
    LoopbackTransportAdapter op_provider_transport {};
    op_consumer_transport.SetPeer(&op_provider_transport);
    op_provider_transport.SetPeer(&op_consumer_transport);

    // Rear lighting transport pair: controller consumer ↔ rear node provider.
    LoopbackTransportAdapter rear_consumer_transport {};
    LoopbackTransportAdapter rear_provider_transport {};
    rear_consumer_transport.SetPeer(&rear_provider_transport);
    rear_provider_transport.SetPeer(&rear_consumer_transport);

    // Build the chain bottom-up.
    RearLightingFunctionManager rear_fn_mgr {};
    RearLightingServiceProvider rear_provider {rear_fn_mgr, rear_provider_transport};
    RearLightingServiceConsumer rear_consumer {rear_consumer_transport};
    CentralZoneController controller {rear_consumer};
    OperatorServiceProvider op_provider {controller, op_provider_transport};
    OperatorServiceConsumer op_consumer {op_consumer_transport, kHmiControlPanelApplicationId};

    RecordingEventListener listener {};
    op_consumer.SetEventListener(&listener);

    // Initialize bottom-up so that each provider transport handler is
    // registered before the corresponding consumer transport fires its
    // availability callback.
    ASSERT_EQ(rear_provider.Initialize(), ServiceStatus::kSuccess);
    ASSERT_EQ(op_provider.Initialize(), OperatorServiceStatus::kSuccess);
    ASSERT_EQ(controller.Initialize(), ControllerStatus::kSuccess);
    ASSERT_EQ(op_consumer.Initialize(), OperatorServiceStatus::kSuccess);

    // Trigger the full round-trip:
    //   op_consumer  →[loopback A]→  op_provider  →  CZC
    //   →  rear_consumer  →[loopback B]→  rear_provider  →  fn_mgr
    //   ←[event B]←  rear_consumer  ←  CZC observer  ←[event A]←  op_consumer
    //   → RecordingEventListener::OnLampStatusUpdated
    //
    // Because both loopbacks are synchronous, the entire chain completes
    // inline before RequestLampToggle returns.
    EXPECT_EQ(
        op_consumer.RequestLampToggle(LampFunction::kParkLamp),
        OperatorServiceStatus::kSuccess);

    EXPECT_EQ(listener.lamp_status_update_count_, 1U);
    EXPECT_EQ(listener.last_lamp_status_.function, LampFunction::kParkLamp);
    EXPECT_EQ(listener.last_lamp_status_.output_state, LampOutputState::kOn);

    // Tear down in reverse order; controller.Shutdown() joins the health
    // poll thread before shutting down the rear service consumer.
    EXPECT_EQ(op_consumer.Shutdown(), OperatorServiceStatus::kSuccess);
    EXPECT_EQ(controller.Shutdown(), ControllerStatus::kSuccess);
    EXPECT_EQ(op_provider.Shutdown(), OperatorServiceStatus::kSuccess);
    EXPECT_EQ(rear_provider.Shutdown(), ServiceStatus::kSuccess);
}

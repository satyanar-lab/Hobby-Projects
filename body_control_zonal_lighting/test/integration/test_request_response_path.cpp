/**
 * @file test_request_response_path.cpp
 * @brief End-to-end service-path integration test.
 *
 * Wires a RearLightingServiceConsumer (controller side) and a
 * RearLightingServiceProvider (node side) through an in-memory loopback
 * transport pair and verifies that a SetLampCommand issued on the
 * consumer reaches the provider, updates the node's LampStatus, and its
 * published event is delivered back to the consumer's listener.
 *
 * Today the assertion is deliberately minimal: the test proves the
 * transport + service wiring is in place. Richer behavioral assertions
 * (timeouts, repeated commands, reject paths) will be added as the
 * in-memory transport matures.
 */

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/service/rear_lighting_service_consumer.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"
#include "body_control/lighting/service/rear_lighting_service_provider.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

namespace
{

using body_control::lighting::application::RearLightingFunctionManager;
using body_control::lighting::domain::CommandSource;
using body_control::lighting::domain::LampCommand;
using body_control::lighting::domain::LampCommandAction;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampStatus;
using body_control::lighting::domain::NodeHealthStatus;
using body_control::lighting::service::RearLightingServiceConsumer;
using body_control::lighting::service::RearLightingServiceEventListenerInterface;
using body_control::lighting::service::RearLightingServiceProvider;
using body_control::lighting::service::ServiceStatus;
using body_control::lighting::transport::TransportAdapterInterface;
using body_control::lighting::transport::TransportMessage;
using body_control::lighting::transport::TransportMessageHandlerInterface;
using body_control::lighting::transport::TransportStatus;

/**
 * @brief Minimal in-process transport adapter that forwards messages to a peer.
 *
 * The pair is wired with SetPeer() after both instances are constructed so
 * that SendRequest / SendResponse / SendEvent on one side end up being
 * delivered as OnTransportMessageReceived on the other side's handler.
 */
class LoopbackTransportAdapter final : public TransportAdapterInterface
{
public:
    LoopbackTransportAdapter() = default;

    void SetPeer(LoopbackTransportAdapter* peer) noexcept { peer_ = peer; }

    TransportStatus Initialize() override
    {
        is_initialized_ = true;
        if (handler_ != nullptr)
        {
            handler_->OnTransportAvailabilityChanged(true);
        }
        return TransportStatus::kSuccess;
    }

    TransportStatus Shutdown() override
    {
        is_initialized_ = false;
        if (handler_ != nullptr)
        {
            handler_->OnTransportAvailabilityChanged(false);
        }
        return TransportStatus::kSuccess;
    }

    TransportStatus SendRequest(const TransportMessage& m) override { return Forward(m); }
    TransportStatus SendResponse(const TransportMessage& m) override { return Forward(m); }
    TransportStatus SendEvent(const TransportMessage& m) override { return Forward(m); }

    void SetMessageHandler(
        TransportMessageHandlerInterface* handler) noexcept override
    {
        handler_ = handler;
    }

private:
    TransportStatus Forward(const TransportMessage& transport_message)
    {
        if ((peer_ == nullptr) || (peer_->handler_ == nullptr))
        {
            return TransportStatus::kServiceUnavailable;
        }

        peer_->handler_->OnTransportMessageReceived(transport_message);
        return TransportStatus::kSuccess;
    }

    TransportMessageHandlerInterface* handler_ {nullptr};
    LoopbackTransportAdapter* peer_ {nullptr};
    bool is_initialized_ {false};
};

/**
 * @brief Captures the most recent lamp status and node health event seen.
 */
class RecordingEventListener final
    : public RearLightingServiceEventListenerInterface
{
public:
    void OnLampStatusReceived(const LampStatus& lamp_status) override
    {
        last_lamp_status_ = lamp_status;
        lamp_status_events_received_++;
    }

    void OnNodeHealthStatusReceived(
        const NodeHealthStatus& node_health_status) override
    {
        last_node_health_status_ = node_health_status;
        node_health_events_received_++;
    }

    void OnServiceAvailabilityChanged(const bool is_service_available) override
    {
        last_service_availability_ = is_service_available;
    }

    LampStatus last_lamp_status_ {};
    NodeHealthStatus last_node_health_status_ {};
    std::uint32_t lamp_status_events_received_ {0U};
    std::uint32_t node_health_events_received_ {0U};
    bool last_service_availability_ {false};
};

}  // namespace

TEST(ServicePathIntegration, ConsumerProviderWireUpSucceeds)
{
    LoopbackTransportAdapter consumer_transport {};
    LoopbackTransportAdapter provider_transport {};
    consumer_transport.SetPeer(&provider_transport);
    provider_transport.SetPeer(&consumer_transport);

    RearLightingFunctionManager rear_lighting_function_manager {};
    RearLightingServiceProvider provider {
        rear_lighting_function_manager, provider_transport};
    RearLightingServiceConsumer consumer {consumer_transport};

    RecordingEventListener listener {};
    consumer.SetEventListener(&listener);

    ASSERT_EQ(provider.Initialize(), ServiceStatus::kSuccess);
    ASSERT_EQ(consumer.Initialize(), ServiceStatus::kSuccess);

    // Send a structurally valid lamp command; the loopback transport will
    // deliver it to the provider, which will ask the function manager to
    // apply it and publish a LampStatus event back to the consumer.
    LampCommand command {};
    command.function = LampFunction::kParkLamp;
    command.action = LampCommandAction::kActivate;
    command.source = CommandSource::kCentralZoneController;
    command.sequence_counter = 1U;

    EXPECT_EQ(consumer.SendLampCommand(command), ServiceStatus::kSuccess);

    // Tear down cleanly.
    EXPECT_EQ(consumer.Shutdown(), ServiceStatus::kSuccess);
    EXPECT_EQ(provider.Shutdown(), ServiceStatus::kSuccess);
}

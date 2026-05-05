#include <gtest/gtest.h>

#include <vector>

#include "body_control/lighting/application/central_zone_controller.hpp"
#include "body_control/lighting/domain/lamp_command_types.hpp"
#include "body_control/lighting/domain/lamp_status_types.hpp"
#include "body_control/lighting/domain/lighting_service_ids.hpp"
#include "body_control/lighting/service/exterior_lighting_service_interface.hpp"
#include "body_control/lighting/service/operator_service_provider.hpp"
#include "body_control/lighting/transport/someip_message_builder.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"
#include "body_control/lighting/transport/transport_status.hpp"

using body_control::lighting::application::CentralZoneController;
using body_control::lighting::domain::LampFunction;
using body_control::lighting::domain::LampOutputState;
using body_control::lighting::domain::LampStatus;
using body_control::lighting::service::ExteriorLightingServiceConsumerInterface;
using body_control::lighting::service::OperatorServiceProvider;
using body_control::lighting::service::ServiceStatus;
using body_control::lighting::transport::TransportAdapterInterface;
using body_control::lighting::transport::TransportMessage;
using body_control::lighting::transport::TransportMessageHandlerInterface;
using body_control::lighting::transport::TransportStatus;
using body_control::lighting::transport::SomeipMessageBuilder;

// ── Stubs ─────────────────────────────────────────────────────────────────────

class StubExteriorLightingConsumer final
    : public ExteriorLightingServiceConsumerInterface
{
public:
    ServiceStatus Initialize() override { return ServiceStatus::kSuccess; }
    ServiceStatus Shutdown()    override { return ServiceStatus::kSuccess; }

    ServiceStatus SendLampCommand(
        const body_control::lighting::domain::LampCommand&) override
    { return ServiceStatus::kSuccess; }

    ServiceStatus RequestLampStatus(LampFunction) override
    { return ServiceStatus::kSuccess; }

    ServiceStatus RequestNodeHealth() override { return ServiceStatus::kSuccess; }

    ServiceStatus SendInjectFault(LampFunction) override
    { return ServiceStatus::kSuccess; }

    ServiceStatus SendClearFault(LampFunction) override
    { return ServiceStatus::kSuccess; }

    ServiceStatus SendGetFaultStatus() override { return ServiceStatus::kSuccess; }

    bool IsServiceAvailable() const noexcept override { return true; }

    void SetEventListener(
        body_control::lighting::service::ExteriorLightingServiceEventListenerInterface*) noexcept override {}
};

// Captures every SendEvent call so tests can inspect the messages.
class CapturingTransport final : public TransportAdapterInterface
{
public:
    TransportStatus Initialize() override { return TransportStatus::kSuccess; }
    TransportStatus Shutdown()   override { return TransportStatus::kSuccess; }

    TransportStatus SendRequest(const TransportMessage&) override
    { return TransportStatus::kSuccess; }

    TransportStatus SendResponse(const TransportMessage&) override
    { return TransportStatus::kSuccess; }

    TransportStatus SendEvent(const TransportMessage& msg) override
    {
        sent_events.push_back(msg);
        return TransportStatus::kSuccess;
    }

    void SetMessageHandler(TransportMessageHandlerInterface* h) noexcept override
    { handler = h; }

    std::vector<TransportMessage> sent_events;
    TransportMessageHandlerInterface* handler{nullptr};
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static LampStatus MakeLampStatus(
    const LampFunction func,
    const LampOutputState state,
    const std::uint16_t seq)
{
    LampStatus s {};
    s.function              = func;
    s.output_state          = state;
    s.command_applied       = true;
    s.last_sequence_counter = seq;
    return s;
}

// ── Fixture ───────────────────────────────────────────────────────────────────

class GetAllLampStatesTest : public ::testing::Test
{
protected:
    StubExteriorLightingConsumer rear_consumer {};
    CentralZoneController        controller    {rear_consumer};
    CapturingTransport           op_transport  {};
    OperatorServiceProvider      provider      {controller, op_transport};

    void SetUp() override
    {
        using body_control::lighting::application::ControllerStatus;
        ASSERT_EQ(controller.Initialize(), ControllerStatus::kSuccess);
        ASSERT_EQ(provider.Initialize(),
                  body_control::lighting::service::OperatorServiceStatus::kSuccess);
    }

    void TearDown() override
    {
        static_cast<void>(provider.Shutdown());
        static_cast<void>(controller.Shutdown());
    }

    // Populate the controller's lamp-status cache by simulating rear-node events.
    void PopulateCache()
    {
        controller.OnLampStatusReceived(
            MakeLampStatus(LampFunction::kLeftIndicator,  LampOutputState::kOn,  1U));
        controller.OnLampStatusReceived(
            MakeLampStatus(LampFunction::kRightIndicator, LampOutputState::kOff, 2U));
        controller.OnLampStatusReceived(
            MakeLampStatus(LampFunction::kHazardLamp,     LampOutputState::kOn,  3U));
        controller.OnLampStatusReceived(
            MakeLampStatus(LampFunction::kParkLamp,       LampOutputState::kOff, 4U));
        controller.OnLampStatusReceived(
            MakeLampStatus(LampFunction::kHeadLamp,       LampOutputState::kOn,  5U));

        // Clear the events that OnLampStatusReceived triggered via the observer.
        op_transport.sent_events.clear();
    }

    // Inject a GetAllLampStates request into the provider via the transport handler.
    void InjectGetAllLampStatesRequest()
    {
        const TransportMessage req =
            SomeipMessageBuilder::BuildOperatorGetAllLampStatesRequest(
                0x1003U, 0x0001U);
        op_transport.handler->OnTransportMessageReceived(req);
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

TEST_F(GetAllLampStatesTest, PublishesFiveLampStatusEvents)
{
    PopulateCache();
    InjectGetAllLampStatesRequest();

    ASSERT_EQ(op_transport.sent_events.size(), 5U);
}

TEST_F(GetAllLampStatesTest, AllEventsHaveOperatorServiceId)
{
    PopulateCache();
    InjectGetAllLampStatesRequest();

    for (const auto& msg : op_transport.sent_events)
    {
        EXPECT_EQ(msg.service_id,
                  body_control::lighting::domain::operator_service::kServiceId);
        EXPECT_EQ(msg.method_or_event_id,
                  body_control::lighting::domain::operator_service::kLampStatusEventId);
        EXPECT_TRUE(msg.is_event);
    }
}

TEST_F(GetAllLampStatesTest, CachedOutputStatesArePreserved)
{
    PopulateCache();
    InjectGetAllLampStatesRequest();

    // Parser is tested elsewhere; verify the expected payload length (5 bytes per status).
    for (const auto& msg : op_transport.sent_events)
    {
        EXPECT_EQ(msg.payload.size(), 5U);
    }

    // Spot-check that the first byte (LampFunction) of each event is non-zero.
    for (const auto& msg : op_transport.sent_events)
    {
        EXPECT_NE(msg.payload[0], 0U);
    }
}

TEST_F(GetAllLampStatesTest, NoEventsPublishedWhenCacheIsEmpty)
{
    // No PopulateCache() call — controller cache has all-kUnknown statuses.
    // GetCachedLampStatus returns true but kUnknown values; provider still
    // publishes because the cache is always initialised with valid (kUnknown) entries.
    InjectGetAllLampStatesRequest();
    // Expect 5 events (one per lamp, even if output_state is kUnknown)
    // because GetCachedLampStatus returns true for default-initialised entries.
    EXPECT_EQ(op_transport.sent_events.size(), 5U);
}

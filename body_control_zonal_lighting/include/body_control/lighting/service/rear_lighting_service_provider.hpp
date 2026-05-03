#ifndef BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_
#define BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_

#include "body_control/lighting/application/node_health_source_interface.hpp"
#include "body_control/lighting/application/rear_lighting_function_manager.hpp"
#include "body_control/lighting/domain/fault_types.hpp"
#include "body_control/lighting/service/rear_lighting_service_interface.hpp"
#include "body_control/lighting/transport/transport_adapter_interface.hpp"

namespace body_control::lighting::service
{

/**
 * Rear-lighting service provider, hosted inside the rear lighting node.
 *
 * Handles incoming SOME/IP method calls from the Central Zone Controller
 * and publishes lamp-state and health events back to it.  Three responsibilities:
 *
 *   1. Receives SetLampCommand / GetLampStatus / GetNodeHealth requests via
 *      OnTransportMessageReceived() and dispatches to the appropriate handler.
 *
 *   2. Delegates all lamp-state changes to RearLightingFunctionManager, which
 *      tracks logical on/off state for each function.
 *
 *   3. Publishes LampStatus and NodeHealthStatus events to the transport
 *      at the rates defined in lighting_constants.hpp; the node's main loop
 *      calls BroadcastAllLampStatuses() and BroadcastNodeHealth() on each tick.
 *
 * If a NodeHealthSourceInterface is provided at construction, its snapshot is
 * used for health events.  Otherwise the provider synthesises a minimal
 * snapshot from its own initialised and transport-available flags.
 */
class RearLightingServiceProvider final
    : public transport::TransportMessageHandlerInterface
{
public:
    /**
     * Constructs with synthesised node health (no external health source).
     *
     * Use this constructor in the Linux simulator where the health source
     * is not separately modelled.
     *
     * @param rear_lighting_function_manager  Manages lamp on/off state.
     * @param transport_adapter               Sends and receives SOME/IP messages.
     */
    RearLightingServiceProvider(
        application::RearLightingFunctionManager& rear_lighting_function_manager,
        transport::TransportAdapterInterface& transport_adapter) noexcept;

    /**
     * Constructs with an explicit node health source.
     *
     * Use this constructor on the STM32 target where real Ethernet link
     * state and GPIO fault flags are available.
     *
     * @param node_health_source  Non-owning reference; must outlive the provider.
     */
    RearLightingServiceProvider(
        application::RearLightingFunctionManager& rear_lighting_function_manager,
        transport::TransportAdapterInterface& transport_adapter,
        application::NodeHealthSourceInterface& node_health_source) noexcept;

    /**
     * Registers this provider as the transport message handler and sets the
     * transport as available for outbound event publishing.
     *
     * @return kSuccess or kTransportError.
     */
    ServiceStatus Initialize();

    /**
     * Deregisters from the transport and stops all further event publishing.
     *
     * @return kSuccess; never fails.
     */
    ServiceStatus Shutdown();

    /**
     * Encodes and publishes the current status of every lamp function as
     * individual LampStatus events over the transport.
     *
     * Called by the node's main loop at kLampStatusPublishPeriod (100 ms).
     */
    void BroadcastAllLampStatuses();

    /**
     * Encodes and publishes the current node health snapshot as a
     * NodeHealthStatus event over the transport.
     *
     * Called by the node's main loop at kNodeHealthPublishPeriod (1 s).
     */
    void BroadcastNodeHealth();

    /**
     * Entry point for all inbound datagrams from the transport adapter.
     *
     * Routes each message to the appropriate handler based on its SOME/IP
     * method ID; unknown IDs are silently dropped.
     */
    void OnTransportMessageReceived(
        const transport::TransportMessage& transport_message) override;

    /** Updates the internal availability flag; suppresses event publishing when false. */
    void OnTransportAvailabilityChanged(
        bool is_available) override;

private:
    /** Decodes a SetLampCommand payload and passes it to the function manager. */
    void HandleSetLampCommand(
        const transport::TransportMessage& transport_message);

    /** Reads lamp status from the function manager and sends a unicast response. */
    void HandleGetLampStatus(
        const transport::TransportMessage& transport_message);

    /** Reads the health snapshot and sends a unicast response. */
    void HandleGetNodeHealth(
        const transport::TransportMessage& transport_message);

    /** Decodes a FaultCommand and injects the fault via the function manager. */
    void HandleInjectFault(
        const transport::TransportMessage& transport_message);

    /** Decodes a FaultCommand and clears the fault via the function manager. */
    void HandleClearFault(
        const transport::TransportMessage& transport_message);

    /** Returns the current LampFaultStatus snapshot to the requester. */
    void HandleGetFaultStatus(
        const transport::TransportMessage& transport_message);

    /** Encodes a LampStatus and fires it as a SOME/IP event on the transport. */
    void PublishLampStatusEvent(
        const domain::LampStatus& lamp_status);

    /** Encodes a NodeHealthStatus and fires it as a SOME/IP event on the transport. */
    void PublishNodeHealthEvent(
        const domain::NodeHealthStatus& node_health_status);

    /** Encodes a LampFaultStatus and fires it as a SOME/IP event on the transport. */
    void PublishFaultStatusEvent(
        const domain::LampFaultStatus& fault_status);

    /**
     * Assembles the current NodeHealthStatus from either the external source
     * (if one was provided) or the provider's own state flags.
     */
    [[nodiscard]] domain::NodeHealthStatus BuildCurrentNodeHealthStatus()
        const noexcept;

    /** Maps a TransportStatus to the equivalent ServiceStatus. */
    static ServiceStatus ConvertTransportStatus(
        transport::TransportStatus transport_status) noexcept;

    application::RearLightingFunctionManager& rear_lighting_function_manager_;
    transport::TransportAdapterInterface& transport_adapter_;

    /// Optional external health source; nullptr when using synthesised health.
    application::NodeHealthSourceInterface* node_health_source_;

    bool is_initialized_;
    bool is_transport_available_;
};

}  // namespace body_control::lighting::service

#endif  // BODY_CONTROL_LIGHTING_SERVICE_REAR_LIGHTING_SERVICE_PROVIDER_HPP_

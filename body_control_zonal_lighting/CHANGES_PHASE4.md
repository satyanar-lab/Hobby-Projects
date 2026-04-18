# Phase 4 — Operator Service Layer

## Overview

Phase 4 introduces the **operator service**: a dedicated UDP service path
between the HMI / diagnostic-console processes and the central zone
controller.  The HMI and diagnostic console no longer embed a
`CentralZoneController` in-process; they are now thin operator clients that
hold an `OperatorServiceConsumer` and communicate with the controller over
two UDP ports (41002 / 41003).  The controller hosts an
`OperatorServiceProvider` that receives lamp requests, applies arbitration,
and fans `LampStatus` + `NodeHealth` events back to all connected clients.

---

## Files changed

### `include/body_control/lighting/service/operator_service_interface.hpp` *(new)*

Defines the operator service public contract:

- `OperatorServiceStatus` — scoped enum with `kSuccess`, `kNotInitialized`,
  `kNotAvailable`, `kInvalidArgument`, `kRejected`, `kTransportError`.
- `OperatorServiceEventListenerInterface` — callback interface for
  `OnLampStatusUpdated`, `OnNodeHealthUpdated`,
  `OnControllerAvailabilityChanged`; delivered to HMI / diagnostic-console
  clients.
- `OperatorServiceProviderInterface` — abstract service interface implemented
  by both `OperatorServiceProvider` (in-process) and
  `OperatorServiceConsumer` (out-of-process proxy); callers hold a pointer to
  this interface so the binding is an implementation detail.

---

### `include/body_control/lighting/service/operator_service_provider.hpp` *(new)*

Declares `OperatorServiceProvider`, the controller-side operator service
host.  Implements both `TransportMessageHandlerInterface` (receives lamp
requests from operator clients) and `RearLightingServiceEventListenerInterface`
(registered as a `CentralZoneController` status observer so it can fan
`LampStatus` / `NodeHealth` events to connected clients).

---

### `src/service/operator_service_provider.cpp` *(new)*

Implements `OperatorServiceProvider`:

- `Initialize` registers itself as the controller's status observer, sets the
  transport handler, and initialises the operator transport.
- `HandleLampToggleRequest`, `HandleLampActivateRequest`,
  `HandleLampDeactivateRequest` parse the incoming `TransportMessage` and
  forward to `CentralZoneController::SendLampCommand`.
- `HandleNodeHealthRequest` calls `CentralZoneController::RequestNodeHealth`.
- `PublishLampStatusEvent` / `PublishNodeHealthEvent` encode and send events
  back to operator clients via the operator transport.

---

### `include/body_control/lighting/service/operator_service_consumer.hpp` *(new)*

Declares `OperatorServiceConsumer`, the HMI / diagnostic-console proxy.
Implements `OperatorServiceProviderInterface` (so callers use the same
interface regardless of in-process or out-of-process binding) and
`TransportMessageHandlerInterface` (receives events from the controller).
Maintains a local cache of up to five `LampStatus` entries and a
`NodeHealthStatus` so callers can read last-known state without polling.

---

### `src/service/operator_service_consumer.cpp` *(new)*

Implements `OperatorServiceConsumer`:

- `RequestLampToggle` / `RequestLampActivate` / `RequestLampDeactivate` /
  `RequestNodeHealth` build the appropriate `TransportMessage` and call
  `transport_.SendRequest`.
- `OnTransportMessageReceived` parses incoming operator events, updates the
  local cache, and fires the registered `OperatorServiceEventListenerInterface`
  callbacks.
- `OnTransportAvailabilityChanged` forwards controller availability to the
  event listener.

---

### `include/body_control/lighting/domain/lighting_service_ids.hpp`

Added `namespace operator_service` containing service / method / event IDs
and port assignments for the operator service path:

| Constant | Value | Purpose |
|---|---|---|
| `kServiceId` | `0x5200` | Operator service identifier |
| `kRequestLampToggleMethodId` | `0x0001` | Toggle request method |
| `kRequestLampActivateMethodId` | `0x0002` | Activate request method |
| `kRequestLampDeactivateMethodId` | `0x0003` | Deactivate request method |
| `kRequestNodeHealthMethodId` | `0x0004` | Node health request method |
| `kLampStatusEventId` | `0x8001` | Lamp status push event |
| `kNodeHealthEventId` | `0x8002` | Node health push event |
| `kHmiControlPanelApplicationId` | `0x1003` | HMI client ID |
| `kDiagnosticConsoleApplicationId` | `0x1004` | Diagnostic console client ID |
| `kControllerOperatorApplicationId` | `0x1005` | Controller operator server ID |
| `kControllerOperatorPort` | `41002` | Controller receive socket |
| `kOperatorClientPort` | `41003` | HMI / diag-console receive socket |

---

### `include/body_control/lighting/application/central_zone_controller.hpp`

Added `SetStatusObserver(RearLightingServiceEventListenerInterface* observer)`
— registers a single observer that is notified inline after every cache
update (`OnLampStatusReceived`, `OnNodeHealthStatusReceived`,
`OnServiceAvailabilityChanged`).  Intended for `OperatorServiceProvider` so
it can relay events to connected operator clients without polling.

---

### `src/application/central_zone_controller.cpp`

Implemented `SetStatusObserver` and added observer call-through in
`OnLampStatusReceived`, `OnNodeHealthStatusReceived`, and
`OnServiceAvailabilityChanged` — each fires
`status_observer_->On*()` after updating the controller's own cache.

---

### `include/body_control/lighting/transport/someip_message_builder.hpp`

Added operator-service builder declarations:
`BuildOperatorLampToggleRequest`, `BuildOperatorLampActivateRequest`,
`BuildOperatorLampDeactivateRequest`, `BuildOperatorNodeHealthRequest`,
`BuildOperatorLampStatusEvent`, `BuildOperatorNodeHealthEvent`.

---

### `include/body_control/lighting/transport/someip_message_parser.hpp`

Added operator-service parser declarations:
`IsOperatorLampToggleRequest`, `IsOperatorLampActivateRequest`,
`IsOperatorLampDeactivateRequest`, `IsOperatorNodeHealthRequest`,
`IsOperatorLampStatusEvent`, `IsOperatorNodeHealthEvent`.

---

### `src/transport/someip_message_builder.cpp`

Implemented the six operator-service builder functions.  Operator request
messages carry a single `LampFunction` byte payload; operator events reuse
the existing `LampStatus` and `NodeHealthStatus` payload layouts.

---

### `src/transport/someip_message_parser.cpp`

Implemented the six operator-service parser / discriminator functions using
`service_id == kOperatorServiceId` + `method_or_event_id` to identify
message type.

---

### `src/transport/ethernet/udp_transport_adapter.cpp`

Extended to handle the operator service port pair (41002 / 41003) in
addition to the existing rear lighting port pair (41000 / 41001).

---

### `src/transport/vsomeip/vsomeip_operator_server_adapter.cpp` *(new)*

vsomeip facade for the controller-side operator service transport (server
role, binds :41002).  Delegates to the ethernet UDP backend for this phase.

---

### `src/transport/vsomeip/vsomeip_operator_client_adapter.cpp` *(new)*

vsomeip facade for the HMI / diagnostic-console operator transport (client
role, binds :41003).  Provides `CreateOperatorClientVsomeipClientAdapter()`
factory used by both operator client `main()` files.

---

### `src/transport/vsomeip/vsomeip_runtime_manager.cpp`

Extended to expose `CreateOperatorClientVsomeipClientAdapter` factory so
`hmi_control_panel` and `diagnostic_console` can obtain a transport without
knowing the backend.

---

### `src/CMakeLists.txt`

Added `operator_service_consumer.cpp`, `vsomeip_operator_client_adapter.cpp`,
and `vsomeip_operator_server_adapter.cpp` to the core library source list.

---

### `app/central_zone_controller/main.cpp`

Rewritten to create and own `OperatorServiceProvider` alongside the existing
`CentralZoneController` and rear-lighting service consumer.  Initialises the
operator transport (server adapter on :41002) and wires it to the provider
before calling `controller.Initialize()`.

---

### `app/hmi_control_panel/main.cpp`

Rewritten to hold `OperatorServiceConsumer` (via `OperatorServiceProviderInterface`)
instead of an embedded `CentralZoneController`.  Creates a vsomeip operator
client adapter (client role, :41003), constructs the consumer with
`kHmiControlPanelApplicationId`, and registers `MainWindow` as the event
listener.

---

### `app/diagnostic_console/main.cpp`

Rewritten to hold `OperatorServiceConsumer` instead of an embedded
`CentralZoneController`.  Constructs a local `DiagnosticEventListener`
that implements `OperatorServiceEventListenerInterface` and prints events
as they arrive.

---

### `include/body_control/lighting/hmi/main_window.hpp`

Updated constructor to take `OperatorServiceProviderInterface&` instead of
`CentralZoneController&`.  `MainWindow` now implements
`OperatorServiceEventListenerInterface` (previously
`RearLightingServiceEventListenerInterface`) so it can receive push events
from the operator consumer.

---

### `src/hmi/main_window.cpp`

Updated to use `OperatorServiceProviderInterface` for all lamp and health
requests.  Added a `RequestLampStatus` call after every toggle so the view
model reflects the confirmed state rather than an optimistic prediction.

---

### `test/integration/test_operator_service_roundtrip.cpp` *(new)*

Integration test: wires the full six-component chain through two synchronous
`LoopbackTransportAdapter` pairs.  Asserts that
`OperatorServiceConsumer::RequestLampToggle(kParkLamp)` delivers exactly one
`OnLampStatusUpdated(kParkLamp, kOn)` callback to the recording listener.

---

### `test/integration/test_controller_arbitration_via_operator.cpp` *(new)*

Integration test: uses the same wiring.  Activates the hazard lamp via the
operator consumer, then sends a `RequestLampToggle(kLeftIndicator)`.  Asserts
that the CZC arbitrator silently rejects the indicator command — no second
`OnLampStatusUpdated` fires and the last-seen function remains `kHazardLamp`.

---

### `test/integration/CMakeLists.txt`

Added `test_operator_service_roundtrip` and
`test_controller_arbitration_via_operator` as GoogleTest targets.

---

## What is unchanged

All seven pre-Phase-4 GoogleTest cases remain green (9/9 total after Phase 4).
The rear lighting service interface, codec, domain types, and STM32 build
path are unmodified.  The `rear_lighting_node_simulator` is unchanged.

# Phase 2 — Core Logic and Service Path

## Overview

Phase 2 locks the domain model, wires the full request/response and
pub/sub service path in memory, and establishes the initial GoogleTest
suite under CTest.  At the end of this phase the four executable targets
exist and can be linked; the rear lighting service path is exercised by
tests, though cross-process communication waits for Phase 3.

---

## Domain layer (`include/body_control/lighting/domain/`)

### `lamp_command_types.hpp`

Defines the `LampCommand` value type sent from controller to rear node:

| Field | Type | Meaning |
|---|---|---|
| `function` | `LampFunction` | Which lamp is being commanded |
| `action` | `LampCommandAction` | Activate / deactivate / toggle / no-action |
| `source` | `CommandSource` | Who issued the command |
| `sequence_counter` | `uint16_t` | Monotonically increasing; consumer uses this to correlate request → outcome |

### `lamp_status_types.hpp`

Defines `LampStatus` (rear node → controller) and `NodeHealthStatus`
(rear node → controller):

`LampStatus` fields: `function`, `output_state` (`LampOutputState`),
`command_applied` (bool), `last_sequence_counter`.

`NodeHealthStatus` fields: `health_state` (`NodeHealthState`),
`ethernet_link_available`, `service_available`,
`lamp_driver_fault_present`, `active_fault_count`.

### `lighting_payload_codec.hpp` / `lighting_payload_codec.cpp`

Five encode/decode function pairs (one per domain struct) that serialise
and deserialise the fixed-length big-endian payloads used on the wire:

| Function pair | Payload size |
|---|---|
| `EncodeLampCommand` / `DecodeLampCommand` | 5 bytes |
| `EncodeLampStatus` / `DecodeLampStatus` | 5 bytes |
| `EncodeNodeHealthStatus` / `DecodeNodeHealthStatus` | 6 bytes |

All decode functions validate enum ranges before populating the output
struct; on any validation failure the output is left unchanged and
`kInvalidPayloadValue` is returned.  A null pointer or wrong-length
buffer returns `kInvalidPayloadLength`.

### `lighting_service_ids.hpp`

Centralises all service, method, event, and application IDs for the rear
lighting service (namespace `rear_lighting_service`, service ID `0x5100`).
A zero-valued ID is never assigned so a zeroed message is structurally
distinguishable from a valid one.

### `lighting_constants.hpp`

Centralises all timing constants:

| Constant | Value | Purpose |
|---|---|---|
| `kMainLoopPeriod` | 20 ms | Periodic tick for simulator and health monitor |
| `kLampStatusPublishPeriod` | 100 ms | Cyclic LampStatus broadcast |
| `kNodeHealthPublishPeriod` | 1000 ms | Cyclic NodeHealthStatus broadcast |
| `kNodeHeartbeatTimeout` | 2000 ms | Controller → declares node unavailable |

---

## Application layer (`include/body_control/lighting/application/`)

### `command_arbitrator.hpp` / `command_arbitrator.cpp`

`CommandArbitrator::Arbitrate()` enforces three rules synchronously on
every incoming `LampCommand`:

1. Structural validity — `function != kUnknown`, `action != kNoAction`.
2. Hazard priority — indicator activation rejected while hazard is active.
3. Indicator deactivation always accepted even while hazard is active.

Returns an `ArbitrationDecision` containing `result` (`kAccepted` /
`kRejected`) and a copy of the accepted command.

### `lamp_state_manager.hpp` / `lamp_state_manager.cpp`

In-process cache of the most recently received `LampStatus` for each of
the five managed lamp functions.  Pre-seeded at construction with
`kUnknown` output state so reads are always valid before the first event.

`GetArbitrationContext()` derives an `ArbitrationContext` from the cache
so `CommandArbitrator` can make priority decisions without holding a
reference to the cache.

`IsFunctionActive(func)` returns true when the cached `output_state` is
`kOn`.

`Reset()` returns all entries to their initial `kUnknown` shape.

### `node_health_monitor.hpp` / `node_health_monitor.cpp`

Tracks node health over time with a heartbeat timeout.

- `UpdateNodeHealthStatus()` records the latest snapshot and resets the
  elapsed timer.
- `ProcessMainLoop(elapsed)` accumulates elapsed time; if it exceeds
  `kNodeHeartbeatTimeout` without an update, the cached state transitions
  to `kUnavailable`.
- `SetServiceAvailability(false)` also forces `kUnavailable` immediately.
- `IsNodeAvailable()` returns true only when health state is known and
  not `kUnavailable`, and both `ethernet_link_available` and
  `service_available` are set.

### `rear_lighting_function_manager.hpp` / `rear_lighting_function_manager.cpp`

Node-side command executor.  `ApplyCommand()` validates the command,
updates the stored `LampStatus` for the function (output state + sequence
counter + `command_applied = true`), and returns false on any validation
failure.

### `central_zone_controller.hpp` / `central_zone_controller.cpp`

Orchestrates the controller side.  Owns a `LampStateManager`,
`NodeHealthMonitor`, and a reference to `RearLightingServiceConsumerInterface`.
`SendLampCommand()` runs arbitration and, on acceptance, calls the service
consumer.  The health-poll background thread calls `RequestNodeHealth()`
on the service consumer every `kNodeHealthPublishPeriod`.

---

## Service layer (`include/body_control/lighting/service/`)

### `rear_lighting_service_interface.hpp`

Defines the public contracts:

- `RearLightingServiceConsumerInterface` — `SendLampCommand`,
  `RequestLampStatus`, `RequestNodeHealth`, `Initialize`, `Shutdown`,
  `SetEventListener`.
- `RearLightingServiceEventListenerInterface` — `OnLampStatusReceived`,
  `OnNodeHealthStatusReceived`, `OnServiceAvailabilityChanged`.
- `ServiceStatus` — scoped enum for service-layer return codes.

### `rear_lighting_service_consumer.hpp` / `.cpp`

Proxy that translates service calls into `TransportMessage` objects and
dispatches incoming messages (events and responses) to the registered
`RearLightingServiceEventListenerInterface`.

### `rear_lighting_service_provider.hpp` / `.cpp`

Receives `TransportMessage` objects from the transport adapter and calls
the corresponding `RearLightingFunctionManager` method.  After applying
a command it publishes the resulting `LampStatus` via
`TransportAdapter::SendEvent`.  `BroadcastAllLampStatuses()` and
`BroadcastNodeHealth()` support the periodic publish loop added in Phase 3.

---

## Transport layer

### `transport_adapter_interface.hpp`

Defines `TransportAdapterInterface` — `Initialize`, `Shutdown`,
`SendRequest`, `SendResponse`, `SendEvent`, `SetMessageHandler` — and
`TransportMessageHandlerInterface` with `OnTransportMessageReceived` and
`OnTransportAvailabilityChanged`.

### `someip_message_builder.hpp` / `someip_message_parser.hpp`

Builder and parser for the SOME/IP-shaped wire format used on the rear
lighting service path.  Builder functions produce a `TransportMessage`;
parser predicates and extractors consume one.

### `ethernet/udp_transport_adapter.hpp` / `.cpp`

Berkeley-sockets UDP backend for Linux.  Binds a receive socket and
spawns a `ReceiverLoop` thread.  Implements the full
`TransportAdapterInterface`.

---

## Test layer

### `test/unit/test_command_arbitrator.cpp`

Ten `TEST` cases covering all `CommandArbitrator` arbitration rules
including hazard expansion to three commands and indicator exclusivity.

### `test/unit/test_lamp_state_manager.cpp`

Seven `TEST` cases covering the cache round-trip, unknown-function
rejection, `IsFunctionActive`, `GetArbitrationContext`, and `Reset`.

### `test/unit/test_lighting_payload_codec.cpp`

Nine `TEST` cases covering encode/decode round-trips and all decode
rejection paths (wrong length, null pointer, out-of-range enum, invalid
boolean byte).

### `test/unit/test_rear_lighting_function_manager.cpp`

Seven `TEST` cases: initial state `kOff`, activate/deactivate/toggle
behavior, `kNoAction` and unknown function rejection, `GetLampStatus`
unknown rejection.

### `test/integration/test_request_response_path.cpp`

Wires `RearLightingServiceConsumer` + `RearLightingServiceProvider`
through a synchronous `LoopbackTransportAdapter`.  Asserts that
`SendLampCommand(kParkLamp, kActivate)` delivers exactly one
`OnLampStatusReceived(kParkLamp, kOn)` callback to the recording listener.

### `test/integration/test_hazard_priority_behavior.cpp`

Wires `CommandArbitrator` + `LampStateManager` without a transport.
Confirms hazard-active context blocks indicator activation and clear
context accepts it.

### `test/integration/test_node_timeout_behavior.cpp`

Exercises `NodeHealthMonitor` timeout: fresh monitor reports unavailable,
operational update marks available, `kNodeHeartbeatTimeout + 1 ms` forces
`kUnavailable`, and `SetServiceAvailability(false)` also forces unavailable.

---

## CMake

`BODY_CONTROL_LIGHTING_WARNINGS_AS_ERRORS` defaults to `ON`.  The test
helper macro `body_control_lighting_add_test()` in
`test/unit/CMakeLists.txt` and `test/integration/CMakeLists.txt`
registers each `.cpp` as its own CTest executable.

---

## What is unchanged from Phase 1

Platform drivers, vsomeip stubs, and STM32 build paths are unmodified.
The domain/service/transport interface signatures established in this
phase are the contract that all later phases build on.

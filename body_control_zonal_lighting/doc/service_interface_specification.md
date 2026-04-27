# Service Interface Specification

This document covers both service paths in the system.

---

# Part A — Rear Lighting Service

**Service ID:** `0x5100`
**Instance ID:** `0x0001`
**Scope:** the contract between the Central Zone Controller (consumer) and
the rear lighting node (provider — Linux simulator or STM32 target).

IDs and layouts in this document are the authoritative contract. Any
change here **must** be reflected in
`include/body_control/lighting/domain/lighting_service_ids.hpp` and in
both the SOME/IP builder/parser and the payload codec, and covered by
the corresponding test in `test/unit/test_lighting_payload_codec.cpp`.

---

## A.1 Service participants

| Application | Application ID | Role |
|---|---|---|
| Central zone controller | `0x1001` | Service consumer |
| Rear lighting node simulator / STM32 target | `0x2001` | Service provider |

The HMI and diagnostic console do not consume the rear lighting service
directly. They connect through the operator service (Part B) and the
Central Zone Controller acts as their proxy to this path.

## A.2 Methods (request / response)

### A.2.1 `SetLampCommand`

| Property | Value |
|---|---|
| Method ID | `0x0001` |
| Direction | Consumer → Provider |
| Reliable | Yes |
| Request payload | `LampCommand` (5 bytes) |
| Response payload | None (outcome reflected in the next `LampStatusEvent`) |

Request payload layout (big-endian):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `function` | `LampFunction` enum |
| 1 | 1 | `action` | `LampCommandAction` enum |
| 2 | 1 | `source` | `CommandSource` enum |
| 3 | 2 | `sequence_counter` | `uint16_t` |

Semantics:

- The provider MUST apply a structurally valid command before publishing
  the resulting `LampStatusEvent`.
- A command with `action == kNoAction` or `function == kUnknown` MUST be
  rejected without publishing an event.
- The provider does not arbitrate priority; that is the consumer's
  responsibility.  Commands that reach the provider have already been
  arbitrated by `CommandArbitrator` on the controller side.

### A.2.2 `GetLampStatus`

| Property | Value |
|---|---|
| Method ID | `0x0002` |
| Direction | Consumer → Provider |
| Request payload | `LampFunction` (1 byte) |
| Response payload | Delivered as `LampStatusEvent` for the requested function |

### A.2.3 `GetNodeHealth`

| Property | Value |
|---|---|
| Method ID | `0x0003` |
| Direction | Consumer → Provider |
| Request payload | Empty |
| Response payload | Delivered as `NodeHealthStatusEvent` |

## A.3 Events (publish / subscribe)

### A.3.1 `LampStatusEvent`

| Property | Value |
|---|---|
| Event ID | `0x8001` |
| Event group ID | `0x0001` |
| Direction | Provider → Consumer |
| Payload | `LampStatus` (5 bytes) |
| Publish period | `kLampStatusPublishPeriod` = 100 ms |

Payload layout (big-endian):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `function` | `LampFunction` enum |
| 1 | 1 | `output_state` | `LampOutputState` enum |
| 2 | 1 | `command_applied` | `bool` (0 or 1) |
| 3 | 2 | `last_sequence_counter` | `uint16_t` |

`last_sequence_counter` mirrors the counter of the most recent command
actually applied. The consumer uses this to correlate request → outcome.

### A.3.2 `NodeHealthStatusEvent`

| Property | Value |
|---|---|
| Event ID | `0x8002` |
| Event group ID | `0x0002` |
| Direction | Provider → Consumer |
| Payload | `NodeHealthStatus` (6 bytes) |
| Publish period | `kNodeHealthPublishPeriod` = 1000 ms |

Payload layout (big-endian):

| Offset | Size | Field | Type |
|---|---|---|---|
| 0 | 1 | `health_state` | `NodeHealthState` enum |
| 1 | 1 | `ethernet_link_available` | `bool` |
| 2 | 1 | `service_available` | `bool` |
| 3 | 1 | `lamp_driver_fault_present` | `bool` |
| 4 | 2 | `active_fault_count` | `uint16_t` |

Source-of-truth on the provider side: if a `NodeHealthSourceInterface`
is injected, its snapshot is authoritative. Otherwise the provider
synthesises a minimal snapshot and reports `kDegraded` when either
transport state is unknown.

---

# Part B — Operator Service

**Service ID:** `0x5200`
**Instance ID:** `0x0001`
**Scope:** the contract between HMI / diagnostic-console operator clients
(consumers) and the Central Zone Controller (provider).  Operator clients
never see the rear lighting service directly.

---

## B.1 Service participants

| Application | Application ID | UDP port (recv) | Role |
|---|---|---|---|
| Central zone controller | `0x1005` | 41002 | Service provider |
| HMI control panel | `0x1003` | 41003 | Service consumer |
| Diagnostic console | `0x1004` | 41003 | Service consumer |

Multiple operator clients share port 41003; each is distinguished by its
application ID in the SOME/IP header.

## B.2 Methods (operator → controller)

| Method | Method ID | Payload | Description |
|---|---|---|---|
| `RequestLampToggle` | `0x0001` | `LampFunction` (1 byte) | Toggle the specified lamp |
| `RequestLampActivate` | `0x0002` | `LampFunction` (1 byte) | Explicitly activate |
| `RequestLampDeactivate` | `0x0003` | `LampFunction` (1 byte) | Explicitly deactivate |
| `RequestNodeHealth` | `0x0004` | Empty | Request a fresh health snapshot |

## B.3 Events (controller → operators)

| Event | Event ID | Payload | Description |
|---|---|---|---|
| `LampStatusEvent` | `0x8001` | `LampStatus` (5 bytes) | Forwarded from rear lighting service |
| `NodeHealthEvent` | `0x8002` | `NodeHealthStatus` (6 bytes) | Forwarded from rear lighting service |

The payload layouts are identical to the rear lighting service events
(Part A.3) — the controller forwards them verbatim without re-encoding.

---

# Common — Enum value ranges

| Enum | Valid values |
|---|---|
| `LampFunction` | 0 (`kUnknown`), 1 (`kLeftIndicator`), 2 (`kRightIndicator`), 3 (`kHazardLamp`), 4 (`kParkLamp`), 5 (`kHeadLamp`) |
| `LampCommandAction` | 0 (`kNoAction`), 1 (`kActivate`), 2 (`kDeactivate`), 3 (`kToggle`) |
| `CommandSource` | 0 (`kUnknown`), 1 (`kHmiControlPanel`), 2 (`kDiagnosticConsole`), 3 (`kCentralZoneController`) |
| `LampOutputState` | 0 (`kUnknown`), 1 (`kOff`), 2 (`kOn`) |
| `NodeHealthState` | 0 (`kUnknown`), 1 (`kOperational`), 2 (`kDegraded`), 3 (`kFaulted`), 4 (`kUnavailable`) |

Value 0 is reserved on every enum so a zeroed message is never mistaken
for a valid one. Decoders structurally validate enum values before
accepting a payload.

# Common — Availability and liveness

- `RearLightingServiceConsumer` treats the service as **available** once
  the transport adapter has reported availability and `Initialize` has
  succeeded.
- `OperatorServiceConsumer` treats the controller as **available** after
  `OnControllerAvailabilityChanged(true)` fires via the transport.
- The controller's `NodeHealthMonitor` considers the rear node
  **available** only when all of the following are true:
    - A `NodeHealthStatusEvent` has been received at least once.
    - Its `health_state` is not `kUnavailable`.
    - `ethernet_link_available` is true.
    - `service_available` is true.
- If no health update is received for `kNodeHeartbeatTimeout` (2000 ms),
  the monitor transitions the cached state to `kUnavailable` and clears
  the link/service flags.

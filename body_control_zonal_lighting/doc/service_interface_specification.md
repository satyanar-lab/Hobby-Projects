# Service Interface Specification

**Service:** Rear Lighting Service
**Service ID:** `0x5100`
**Instance ID:** `0x0001`
**Document scope:** the public contract exposed by the rear lighting node
— methods, events, parameters, and the state they report on.

IDs and layouts in this document are the authoritative contract. Any
change here **must** be reflected in
`include/body_control/lighting/domain/lighting_service_ids.hpp` and in
both the SOME/IP builder/parser and the payload codec, and covered by
the corresponding test in `test/unit/test_lighting_payload_codec.cpp`.

---

## 1. Service participants

| Application | Application ID | Role |
|---|---|---|
| Central zone controller | `0x1001` | Service consumer |
| HMI control panel | `0x1002` | Service consumer (via controller) |
| Rear lighting node simulator / STM32 target | `0x2001` | Service provider |

The diagnostic console currently connects as a consumer peer of the
controller; it does not own its own application ID.

## 2. Methods (request / response)

### 2.1 `SetLampCommand`

| Property | Value |
|---|---|
| Method ID | `0x0001` |
| Direction | Consumer → Provider |
| Reliable | Yes |
| Request payload | `LampCommand` (5 bytes) |
| Response payload | None (status inferred from `LampStatusEvent`) |

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
  responsibility. Commands that reach the provider have already been
  arbitrated by `CommandArbitrator` on the controller side.

### 2.2 `GetLampStatus`

| Property | Value |
|---|---|
| Method ID | `0x0002` |
| Direction | Consumer → Provider |
| Reliable | Yes |
| Request payload | `LampFunction` (1 byte) |
| Response payload | Delivered as `LampStatusEvent` |

The provider replies by publishing a `LampStatusEvent` for the requested
function.

### 2.3 `GetNodeHealth`

| Property | Value |
|---|---|
| Method ID | `0x0003` |
| Direction | Consumer → Provider |
| Reliable | Yes |
| Request payload | Empty |
| Response payload | Delivered as `NodeHealthStatusEvent` |

## 3. Events (publish / subscribe)

### 3.1 `LampStatusEvent`

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

### 3.2 `NodeHealthStatusEvent`

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

Source-of-truth precedence on the provider side: if a
`NodeHealthSourceInterface` is injected, its snapshot is authoritative.
Otherwise the provider synthesises a minimal snapshot from its own
initialize / transport state and reports `kDegraded` when either is
false.

## 4. Enum value ranges

| Enum | Valid values |
|---|---|
| `LampFunction` | 0 (`kUnknown`), 1 (`kLeftIndicator`), 2 (`kRightIndicator`), 3 (`kHazardLamp`), 4 (`kParkLamp`), 5 (`kHeadLamp`) |
| `LampCommandAction` | 0 (`kNoAction`), 1 (`kActivate`), 2 (`kDeactivate`), 3 (`kToggle`) |
| `CommandSource` | 0 (`kUnknown`), 1 (`kHmiControlPanel`), 2 (`kDiagnosticConsole`), 3 (`kCentralZoneController`) |
| `LampOutputState` | 0 (`kUnknown`), 1 (`kOff`), 2 (`kOn`) |
| `NodeHealthState` | 0 (`kUnknown`), 1 (`kOperational`), 2 (`kDegraded`), 3 (`kFaulted`), 4 (`kUnavailable`) |

Value 0 (`kUnknown` / `kNoAction`) is reserved on every enum so a zeroed
message is never mistaken for a valid one. Decoders structurally validate
enum values before accepting a payload.

## 5. Availability and liveness

- The consumer side (`RearLightingServiceConsumer`) treats the service as
  **available** once the transport adapter has reported availability and
  Initialize has succeeded.
- The controller's `NodeHealthMonitor` considers the node
  **available** only when all of the following are true:
    - A `NodeHealthStatusEvent` has been received at least once.
    - Its `health_state` is not `kUnavailable`.
    - `ethernet_link_available` is true.
    - `service_available` is true.
- If no health update is received for
  `kNodeHeartbeatTimeout` (2000 ms), the monitor transitions the cached
  state to `kUnavailable` and clears the link/service flags.

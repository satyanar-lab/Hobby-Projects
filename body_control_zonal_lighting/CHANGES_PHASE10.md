# Phase 10 — Fault Injection

## Summary

Phase 10 adds a complete fault injection subsystem spanning all layers of
the stack: domain types, application-layer FaultManager, SOME/IP codec
extensions, new method IDs on both service paths, updated apps, and a new
unit test suite.  Faulted lamps refuse activation, are immediately
de-energised when a fault is injected, and resume normal operation when the
fault is cleared.  NodeHealthStatus and the Qt HMI reflect fault state in
real time.

---

## 1 — Domain Layer (`include/body_control/lighting/domain/`)

### New file: `fault_types.hpp`

| Symbol | Purpose |
|---|---|
| `enum class FaultCode : uint16_t` | DTC codes 0xB001–0xB005, one per LampFunction; kNoFault = 0 satisfies the sentinel rule |
| `enum class FaultAction : uint8_t` | kInject = 1, kClear = 2 |
| `struct FaultCommand` | Wire type for inject/clear method calls; same 8-byte layout as LampCommand |
| `struct LampFaultStatus` | Fault snapshot: fault_present, active_fault_count, std::array<FaultCode, 5> |
| `LampFunctionToFaultCode()` | Maps LampFunction → FaultCode |
| `FaultCodeToString()` | Human-readable DTC label |
| `IsValidFaultCommand()` | Structural validator, used by codec |

### Updated: `lighting_service_ids.hpp`

`rear_lighting_service` namespace:
- `kInjectLampFaultMethodId = 0x0004`
- `kClearLampFaultMethodId  = 0x0005`
- `kGetFaultStatusMethodId  = 0x0006`
- `kFaultStatusEventId      = 0x8003`

`operator_service` namespace:
- `kRequestInjectFaultMethodId    = 0x0005`
- `kRequestClearFaultMethodId     = 0x0006`
- `kRequestGetFaultStatusMethodId = 0x0007`

### Updated: `lighting_payload_codec.hpp` / `.cpp`

Two new fixed-payload types and four new codec functions:
- `EncodeFaultCommand` / `DecodeFaultCommand` — 8-byte payload, same layout as LampCommand
- `EncodeLampFaultStatus` / `DecodeLampFaultStatus` — 16-byte payload (fault_present + count + 5 × uint16 DTC codes)

---

## 2 — Application Layer

### New: `fault_manager.hpp` / `fault_manager.cpp`

Owns the per-lamp fault flag array and the active DTC list.

| Method | Behaviour |
|---|---|
| `InjectFault(LampFunction)` | Sets fault flag, appends DTC; returns kAlreadyFaulted if no-op |
| `ClearFault(LampFunction)` | Clears flag, removes DTC (compacted); returns kNotFaulted if no-op |
| `ClearAllFaults()` | Resets all flags and DTC slots |
| `IsFaulted(LampFunction)` | Queried by RearLightingFunctionManager::ApplyCommand |
| `GetFaultStatus()` | Returns LampFaultStatus snapshot |
| `PopulateHealth(NodeHealthStatus&)` | Writes lamp_driver_fault_present + active_fault_count; promotes health_state to kFaulted when faults present |

No synchronisation primitives — single-thread affinity contract documented in the header.  Compiles unchanged on Linux, STM32, and Zephyr.

### Updated: `rear_lighting_function_manager.hpp` / `.cpp`

- Owns an internal `FaultManager fault_manager_`.
- `ApplyCommand`: guard at the top — kActivate (or kToggle from off) blocked when the target function is faulted.
- `HandleFaultInjection(LampFunction)`: injects fault + forces lamp output kOff.
- `HandleFaultClear(LampFunction)`: clears fault, re-enables future activation.
- `HandleClearAllFaults()`: delegates to FaultManager.
- `GetFaultStatus()`: returns current LampFaultStatus.

Existing unit tests pass unchanged because the default-constructed FaultManager has no active faults.

### Updated: `central_zone_controller.hpp` / `.cpp`

Three new methods forwarding to the rear lighting service consumer:
- `SendInjectFault(LampFunction)`
- `SendClearFault(LampFunction)`
- `SendGetFaultStatus()`

---

## 3 — Transport Layer

### Updated: `someip_message_builder.hpp` / `.cpp`

New build methods:
- `BuildInjectFaultRequest` / `BuildClearFaultRequest` — rear lighting service
- `BuildGetFaultStatusRequest` — rear lighting service
- `BuildFaultStatusEvent` — rear node event
- `BuildOperatorInjectFaultRequest` / `BuildOperatorClearFaultRequest` — operator service
- `BuildOperatorGetFaultStatusRequest` — operator service

### Updated: `someip_message_parser.hpp` / `.cpp`

New predicates and parsers:
- `IsInjectFaultRequest` / `IsClearFaultRequest` / `IsGetFaultStatusRequest` / `IsFaultStatusEvent`
- `ParseFaultCommand` / `ParseLampFaultStatus`
- `IsOperatorInjectFaultRequest` / `IsOperatorClearFaultRequest` / `IsOperatorGetFaultStatusRequest`

---

## 4 — Service Layer

### Updated: `rear_lighting_service_interface.hpp`

Three new pure-virtual methods on `RearLightingServiceConsumerInterface`:
- `SendInjectFault(LampFunction)`
- `SendClearFault(LampFunction)`
- `SendGetFaultStatus()`

### Updated: `rear_lighting_service_provider.hpp` / `.cpp`

New message handlers dispatched from `OnTransportMessageReceived`:
- `HandleInjectFault` — injects fault via function manager, publishes updated lamp status + fault status event + node health event
- `HandleClearFault` — clears fault, publishes fault status + node health events
- `HandleGetFaultStatus` — publishes current fault status event
- `BuildCurrentNodeHealthStatus` — now reads FaultManager state via `GetFaultStatus()` for both synthesised and source-backed paths

### Updated: `rear_lighting_service_consumer.hpp` / `.cpp`

Implements the three new interface methods by building and sending the appropriate SOME/IP request datagrams.

### Updated: `operator_service_interface.hpp`

Three new pure-virtual methods on `OperatorServiceProviderInterface`:
- `RequestInjectFault(LampFunction)`
- `RequestClearFault(LampFunction)`
- `RequestGetFaultStatus()`

### Updated: `operator_service_consumer.hpp` / `.cpp`

Implements the three new methods — serialises requests using the new builder methods.

### Updated: `operator_service_provider.hpp` / `.cpp`

New handlers dispatched from `OnTransportMessageReceived`:
- `HandleInjectFaultRequest` → `controller_.SendInjectFault(func)`
- `HandleClearFaultRequest` → `controller_.SendClearFault(func)`
- `HandleGetFaultStatusRequest` → `controller_.SendGetFaultStatus()`

---

## 5 — Applications

### `app/diagnostic_console/main.cpp`

New menu options:

| Option | Action |
|---|---|
| 12 → Inject fault | Sub-menu selects lamp (1–5); sends RequestInjectFault |
| 13 → Clear fault | Sub-menu selects lamp (1–5); sends RequestClearFault |
| 14 → Clear all faults | Sends RequestClearFault for all five functions |
| 15 → Get fault status | Sends RequestGetFaultStatus; prints fault_present + count from cached NodeHealth |

### `app/hmi_control_panel/qml_hmi_bridge.hpp` / `.cpp`

Two new Q_PROPERTYs (both notify via `faultStatusChanged` signal):
- `bool faultPresent` — true when any lamp fault is active
- `int activeFaultCount` — number of active faults

`OnNodeHealthUpdated` now writes `fault_present` and `active_fault_count` into `pending_`.  `pollAndUpdate` emits `faultStatusChanged` on real transitions.

### `app/hmi_control_panel/LightingHmi.qml`

FAULT row added to the node-health panel:
- Indicator dot: green (dim) when no faults, red when fault present
- Label: `FAULT: N` — green when 0, red when N > 0

---

## 6 — Build System

`src/CMakeLists.txt`: `application/fault_manager.cpp` added to both `BODY_CONTROL_LIGHTING_CORE_SOURCES_COMMON` (Linux) and `BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32`.

---

## 7 — Tests

### New: `test/unit/test_fault_manager.cpp`

17 unit tests covering:
- `InjectFault`: new fault, already-faulted, unknown function
- `ClearFault`: existing fault, not-faulted, unknown function
- Multiple independent faults tracked correctly
- `GetFaultStatus`: no faults, one fault, after clear
- Inject → clear → inject sequence
- `ClearAllFaults` resets everything
- `PopulateHealth` sets health_state, lamp_driver_fault_present, active_fault_count

---

## Result

| Metric | Value |
|---|---|
| Tests | 10/10 passing (9 existing + 1 new) |
| New test cases | 17 |
| Build | Clean (Linux) |
| Existing test regressions | 0 |

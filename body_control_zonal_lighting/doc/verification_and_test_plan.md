# Verification and Test Plan

**Document scope:** how the project is verified — what's tested, how it's
wired into the build, and what the next verification steps are.

---

## 1. Verification strategy at a glance

| Level | Tool | Scope |
|---|---|---|
| Static discipline | `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wold-style-cast -Wnon-virtual-dtor -Werror` | All core, app, and test translation units |
| Engineering comments | Manual pass — every class, public function, non-obvious block | All source files |
| Unit tests | GoogleTest + CTest | Each domain / application component in isolation |
| Integration tests | GoogleTest + CTest | Multi-component wiring through synchronous loopback transport |
| System demo (Linux) | Four executables + vsomeip | End-to-end HMI → controller → node flow |
| Hardware demo (STM32) | NUCLEO-H753ZI + serial logger | GPIO toggling, LwIP UDP, status events to HMI |

Static analysis (cppcheck, clang-tidy with a MISRA profile) is planned
but not yet wired.

## 2. Build-and-test one-liner

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

CTest discovers every `add_test()` registered by
`test/unit/CMakeLists.txt` and `test/integration/CMakeLists.txt`.

## 3. Unit tests (under `test/unit/`)

### 3.1 `test_command_arbitrator.cpp`

Ten cases covering all `CommandArbitrator` rules:

- Rejects `kUnknown` function.
- Rejects `kNoAction`.
- Hazard activation always accepted, even when both indicators are active
  in context; expands to exactly 3 commands (hazard + left + right).
- Hazard deactivation also produces 3 commands.
- Left / right indicator activation rejected while hazard is active.
- Left / right indicator *deactivation* accepted even while hazard is active.
- Park and head lamp commands accepted regardless of hazard state.
- Single-command accepted decision preserves all original command fields.
- Left indicator activation while right is active: `kModified` result,
  2 commands (deactivate right, activate left).
- Right indicator activation while left is active: `kModified` result,
  2 commands (deactivate left, activate right).

### 3.2 `test_lamp_state_manager.cpp`

Seven cases covering the state cache:

- Initial cache: every managed function reports `kUnknown` output state,
  `command_applied == false`, counter 0.
- Update / get round-trip preserves all fields including `command_applied`.
- Update rejects `kUnknown` function.
- Get rejects `kUnknown` function.
- `IsFunctionActive` follows the reported `LampOutputState` after updates.
- `GetArbitrationContext().hazard_lamp_active` tracks hazard lamp state.
- `Reset()` returns the cache to its initial shape.

### 3.3 `test_lighting_payload_codec.cpp`

Nine cases covering the on-wire codec:

- `LampCommand` round-trip (0x1234 sequence counter exercises big-endian
  16-bit encode).
- `LampCommand` decode rejects wrong-length buffer (4 bytes).
- `LampCommand` decode rejects null pointer.
- `LampCommand` decode rejects out-of-range `LampFunction` byte (0xFF).
- `LampStatus` round-trip (0xBEEF sequence counter exercises big-endian
  16-bit decode).
- `LampStatus` decode rejects invalid boolean byte at offset 2 (0xAA —
  neither 0 nor 1).
- `NodeHealthStatus` round-trip.
- `NodeHealthStatus` decode rejects out-of-range `NodeHealthState` (0xEE).
- `NodeHealthStatus` decode rejects wrong-length buffer (3 bytes).

### 3.4 `test_rear_lighting_function_manager.cpp`

Seven cases covering command application:

- Initial outputs are `kOff` for all five managed functions.
- `kActivate` turns the lamp `kOn` and stamps the sequence counter.
- `kDeactivate` turns the lamp `kOff`; sequence counter advances.
- `kToggle` inverts output state (off → on → off).
- `kNoAction` is rejected; state unchanged.
- Unknown function is rejected.
- `GetLampStatus` rejects the unknown function.

## 4. Integration tests (under `test/integration/`)

### 4.1 `test_request_response_path.cpp`

Wires `RearLightingServiceConsumer` + `RearLightingServiceProvider`
through a synchronous `LoopbackTransportAdapter` pair.  Confirms that
`SendLampCommand(kParkLamp, kActivate)` delivers exactly one
`OnLampStatusReceived(kParkLamp, kOn)` callback (with the correct
sequence counter) to the recording listener, inline on the same call
stack.

### 4.2 `test_hazard_priority_behavior.cpp`

Wires `CommandArbitrator` + `LampStateManager` without a transport.
Uses `UpdateLampStatus` on the state manager to drive `GetArbitrationContext`:

- Reporting hazard as `kOn` → indicator activation is `kRejected`.
- Reporting hazard as `kOff` → indicator activation is `kAccepted`.

### 4.3 `test_node_timeout_behavior.cpp`

Exercises `NodeHealthMonitor` heartbeat timeout:

- Fresh monitor reports node unavailable.
- Operational update marks node available.
- `ProcessMainLoop(kNodeHeartbeatTimeout + 1 ms)` without further updates
  forces cached state to `kUnavailable`; `GetNodeHealthStatus` returns
  cleared link/service flags.
- `SetServiceAvailability(false)` after a known-good update also
  transitions to unavailable immediately.

### 4.4 `test_operator_service_roundtrip.cpp`

Wires the full six-component chain through two synchronous loopback
transport pairs:

```
OperatorServiceConsumer ↔ [loopback A] ↔ OperatorServiceProvider
  → CentralZoneController → RearLightingServiceConsumer
  ↔ [loopback B] ↔ RearLightingServiceProvider → RearLightingFunctionManager
```

The loopback adapter uses a mutex on the handler pointer to be safe
against the CZC's background health-poll thread.  Asserts that
`RequestLampToggle(kParkLamp)` delivers exactly one
`OnLampStatusUpdated(kParkLamp, kOn)` callback.

### 4.5 `test_controller_arbitration_via_operator.cpp`

Same six-component wiring.  Activates hazard via the operator consumer
(produces 3 status events, count = 3, last = `kRightIndicator kOn`).
Then sends `RequestLampToggle(kLeftIndicator)` — the CZC arbitrator
rejects it, count stays at 3, and `last_lamp_status_.function` remains
`kRightIndicator`.

## 5. What the tests do not yet cover

- **Cross-process** tests.  All tests run in one process.  A harness that
  spawns the four executables and asserts against their output is tracked
  for a future phase.
- **vsomeip transport** is not exercised by any GoogleTest case.  The
  vsomeip adapters are verified only by running the executables end-to-end.
- **STM32 on-target**: GPIO output assertions and LwIP UDP round-trip
  are verified by observation on hardware, not by automated tests.
- **Static analysis**: cppcheck and clang-tidy with a MISRA-oriented
  profile.  Compiler warnings are the interim gate.
- **Long-soak** stability: multi-hour run checking for drift in health
  state or sequence counters.
- **Qt HMI automated testing**: QML-level interactions are not covered by
  GoogleTest; manual testing via the demo walkthrough is the current gate.

## 6. How to add a new test

1. Drop the `.cpp` under `test/unit/` or `test/integration/`.
2. Include `<gtest/gtest.h>` and whatever application / service headers
   the test exercises.
3. Write one or more `TEST(Suite, CaseName) { ... }` blocks.
4. Add the filename (without path) to
   `test/unit/CMakeLists.txt` or `test/integration/CMakeLists.txt` using
   the `body_control_lighting_add_test(...)` helper.
5. Rebuild and run `ctest --output-on-failure`.

The helper registers the test file as its own executable linked against
`body_control_lighting::core` and `GTest::gtest_main`, and adds it to
CTest.  No extra plumbing is needed.

# Verification and Test Plan

**Document scope:** how the project is verified — what's tested, how it's
wired into the build, and what the next verification steps are.

---

## 1. Verification strategy at a glance

| Level | Tool | Scope |
|---|---|---|
| Static discipline | `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wold-style-cast -Wnon-virtual-dtor` | All core, app, and test translation units |
| Unit tests | GoogleTest + CTest | Each domain / application component in isolation |
| Integration tests | GoogleTest + CTest | Multi-component wiring, including a loopback transport |
| System demo (manual) | The four executables run together | End-to-end HMI → controller → node flow |
| Target verification | STM32 board + serial logger | Reserved for the hardware phase |

Static analysis (cppcheck, clang-tidy with a MISRA profile) is planned
but not yet wired. Until it is, the warning flags above are the guardrail.

## 2. Build-and-test one-liner

```bash
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

CTest will discover every `add_test()` registered by
`test/unit/CMakeLists.txt` and `test/integration/CMakeLists.txt`.

## 3. Unit tests (under `test/unit/`)

### 3.1 `test_command_arbitrator.cpp`
Seven cases covering the `CommandArbitrator` rules in full:

- Rejects `kUnknown` function.
- Rejects `kNoAction`.
- Hazard activation accepted unconditionally (including when indicators
  are active in context).
- Left / right indicator activation rejected when hazard is active.
- Left / right indicator *deactivation* accepted even when hazard is
  active.
- Park and head lamp commands accepted regardless of hazard state.
- Accepted decision preserves the original command (no mutation).

### 3.2 `test_lamp_state_manager.cpp`
Seven cases covering the state cache:

- Initial cache reports every managed function with
  `output_state == kUnknown`, `command_applied == false`, counter 0.
- Update / get round-trip for a managed function.
- Update rejects `kUnknown` function.
- Get rejects `kUnknown` function.
- `IsFunctionActive` mirrors the reported `LampOutputState` after updates.
- `GetArbitrationContext().hazard_lamp_active` tracks the hazard lamp's
  reported state.
- `Reset()` returns the cache to its initial shape.

### 3.3 `test_lighting_payload_codec.cpp`
Nine cases covering the on-wire codec:

- `LampCommand`, `LampStatus`, `NodeHealthStatus` round-trip.
- Decode rejects wrong payload length (too short).
- Decode rejects null payload pointer.
- Decode rejects out-of-range enum values.
- Decode rejects invalid boolean byte.

### 3.4 `test_rear_lighting_function_manager.cpp`
Seven cases covering command application:

- Initial outputs are `kOff` for all managed functions.
- `kActivate` turns the lamp `kOn` and stamps the sequence counter.
- `kDeactivate` turns the lamp `kOff` and stamps the sequence counter.
- `kToggle` inverts the output state.
- `kNoAction` is rejected.
- Unknown function is rejected.
- `GetLampStatus` rejects the unknown function.

## 4. Integration tests (under `test/integration/`)

### 4.1 `test_request_response_path.cpp`
Wires a real `RearLightingServiceConsumer` to a real
`RearLightingServiceProvider` through an in-memory `LoopbackTransportAdapter`
test double. Confirms that:

- Both sides `Initialize()` cleanly.
- `SendLampCommand` on the consumer returns success.
- Both sides `Shutdown()` cleanly.

The test deliberately does not yet assert on the resulting
`LampStatusEvent` — that assertion arrives once the loopback adapter
adds deterministic event-delivery guarantees.

### 4.2 `test_hazard_priority_behavior.cpp`
Wires the real `CommandArbitrator` against an `ArbitrationContext`
derived from a real `LampStateManager`. Confirms that:

- Reporting hazard as active (via an update to the state manager)
  produces `kRejected` for a subsequent indicator activation.
- Reporting hazard as off produces `kAccepted` for the same request.

### 4.3 `test_node_timeout_behavior.cpp`
Exercises the `NodeHealthMonitor` heartbeat timeout:

- A fresh monitor reports the node as unavailable.
- An operational update marks the node available.
- Advancing the main-loop clock by more than
  `kNodeHeartbeatTimeout` forces the cached state to `kUnavailable`.
- `SetServiceAvailability(false)` after a known-good update also
  transitions the monitor to unavailable.

## 5. What the tests do not yet cover

Tracked for upcoming phases:

- **Full consumer/provider event round-trip with assertion** on the
  received `LampStatusEvent` content. Needs the loopback adapter to
  deterministically deliver events.
- **Byte-exact wire compatibility** between `SomeipMessageBuilder /
  Parser` and the real `vsomeip` serialisation (Phase 4).
- **Cross-process** tests. Everything today runs in one process. Phase
  3 will add a test harness that spawns the simulator and controller and
  asserts against their logs.
- **STM32 on-target** verification (Phase 5): GPIO toggling, link
  detection, LwIP UDP round-trip.
- **Static analysis**: cppcheck and clang-tidy with a MISRA-oriented
  profile. Compiler warnings are the interim gate.
- **Long-soak** stability: leave the four executables running for hours
  and check for drift in the health state or sequence counters.

## 6. How to add a new test

1. Drop the `.cpp` under `test/unit/` or `test/integration/`.
2. Include `<gtest/gtest.h>` and whatever application / service headers
   the test exercises.
3. Write one or more `TEST(Suite, CaseName) { ... }` blocks.
4. Add the filename (without path) to
   `test/unit/CMakeLists.txt` or `test/integration/CMakeLists.txt` using
   the `body_control_lighting_add_test(...)` helper.
5. Rebuild and run `ctest --output-on-failure`.

No extra plumbing is needed: the helper registers the test file as its
own executable linked against `body_control_lighting::core` and
`GTest::gtest_main`, and adds it to CTest.

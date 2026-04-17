# Phase 3 — Simulation Integration

## Overview

Phase 3 makes the Linux simulation layer self-contained and verifiable:
the in-process test transport is guaranteed synchronous, the round-trip
integration test asserts the full event path, the rear-node simulator
gains a real periodic publish loop driven by signal-safe shutdown, all
compiler warnings are now promoted to errors, and a shell smoke test
exercises the binary layer end-to-end.

---

## Files changed

### `test/integration/test_request_response_path.cpp`

**LoopbackTransportAdapter — synchronous delivery guarantee**

- `SetMessageHandler` now replays `OnTransportAvailabilityChanged(true)`
  inline if the adapter is already initialised, so the registration order
  no longer affects correctness.
- `Forward` returns `kNotInitialized` early if `is_initialized_` is false,
  eliminating a silent no-op path.
- File-level doc comment updated to document the inline delivery contract.

**ConsumerProviderWireUpSucceeds — round-trip assertions**

Added four `EXPECT_EQ` assertions that fire after `SendLampCommand` returns:

| Assertion | Expected value |
|---|---|
| `lamp_status_events_received_` | `1U` |
| `last_lamp_status_.function` | `LampFunction::kParkLamp` |
| `last_lamp_status_.output_state` | `LampOutputState::kOn` |
| `last_lamp_status_.last_sequence_counter` | `command.sequence_counter` (1U) |

Because the loopback is synchronous these assertions require no sleeps.

---

### `include/body_control/lighting/service/rear_lighting_service_provider.hpp`

Added two public methods:

- `void BroadcastAllLampStatuses()` — iterates all five lamp functions and
  publishes the current `LampStatus` for each via the transport.
- `void BroadcastNodeHealth()` — publishes the current node health snapshot.

---

### `src/service/rear_lighting_service_provider.cpp`

Implemented `BroadcastAllLampStatuses` and `BroadcastNodeHealth`.

- Uses `domain::system_limits::kMaximumLampFunctionCount` for the array
  size constant.
- Added `<array>` and `domain/lighting_constants.hpp` includes.

---

### `app/rear_lighting_node_simulator/main.cpp`

Replaced the `std::cin.get()` blocking wait with a real periodic publish
loop:

- `ProcessSignalHandler::Install()` registers SIGINT / SIGTERM for
  graceful shutdown.
- Main loop sleeps `kMainLoopPeriod` (20 ms) per tick and accumulates
  elapsed time against `kLampStatusPublishPeriod` (100 ms) and
  `kNodeHealthPublishPeriod` (1000 ms).
- On each period boundary the loop calls `BroadcastAllLampStatuses()` or
  `BroadcastNodeHealth()` as appropriate.
- Added includes: `<chrono>`, `<thread>`,
  `body_control/lighting/domain/lighting_constants.hpp`,
  `body_control/lighting/platform/linux/process_signal_handler.hpp`.

---

### `CMakeLists.txt`

`BODY_CONTROL_LIGHTING_WARNINGS_AS_ERRORS` default changed from `OFF` to
`ON`.  The `-Werror` flag (GCC/Clang) and `/WX` (MSVC) are now active for
all builds.  No new warnings surfaced — the existing codebase was already
clean.

---

### `tools/system_smoke_test.sh` *(new)*

End-to-end smoke test script:

1. Starts `rear_lighting_node_simulator` in the background and waits up to
   3 seconds for its "running" banner.
2. Pipes `7\n0\n` into `diagnostic_console` (activate park lamp, then exit)
   and captures stdout.
3. Asserts the output contains `ParkLamp.*On` confirming a full UDP
   request→response round-trip.
4. Cleans up the simulator process and temp log directory on exit (via
   `trap cleanup EXIT`).

The script documents why `central_zone_controller_app` and
`diagnostic_console` cannot run simultaneously (both bind UDP port 41000
without `SO_REUSEPORT`).

---

## What is unchanged

All seven GoogleTest cases remain green.  No domain types, codec, or
transport wire format were modified.  The STM32 build path is unaffected
(the new simulator loop is Linux-only; `BroadcastAllLampStatuses` and
`BroadcastNodeHealth` are platform-neutral additions to the service layer).

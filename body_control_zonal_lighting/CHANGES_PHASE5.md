# Phase 5 — Real vsomeip Transport and Arbitration Fixes

## Overview

Phase 5 replaces the vsomeip stub (which silently delegated to the UDP
ethernet backend) with a real **vsomeip 3.4.10** implementation, fixes two
automotive arbitration bugs, and wires the correct vsomeip environment
variables into the developer run scripts.

The three key deliverables are:

1. **vsomeip transport** — `VsomeipServerAdapterImpl` and
   `VsomeipClientAdapterImpl` implement `TransportAdapterInterface` using
   the vsomeip3 API.  Each adapter runs the vsomeip dispatch loop on a
   dedicated `std::thread`; server adapters offer a SOME/IP service and
   handle method calls; client adapters request a service, register events,
   and subscribe to event groups.

2. **Arbitration correctness** — `CommandArbitrator` now expands hazard
   activate/deactivate into three commands (hazard + left indicator + right
   indicator) and enforces left/right indicator exclusivity (activating one
   auto-deactivates the other).  `LampStateManager::GetArbitrationContext`
   was fixed to populate all five indicator flags instead of only
   `hazard_lamp_active`.

3. **Run scripts** — each script exports the correct
   `VSOMEIP_CONFIGURATION` and `VSOMEIP_APPLICATION_NAME` before launching
   its executable.

---

## External dependency

### vsomeip 3.4.10

Built from source and installed to `/usr/local`:

- Headers: `/usr/local/include/vsomeip/`
- Library: `/usr/local/lib/libvsomeip3.so`
- CMake package: `/usr/local/lib/cmake/vsomeip3/vsomeip3Config.cmake`

No changes to the source tree are required; the installation is detected
at configure time by `find_package(vsomeip3 REQUIRED)`.

---

## Files changed

### `src/CMakeLists.txt`

- Removed the five vsomeip `.cpp` files from
  `BODY_CONTROL_LIGHTING_CORE_SOURCES_LINUX`.
- Added a Linux-only block that:
  - Appends `/usr/local` to `CMAKE_PREFIX_PATH`.
  - Calls `find_package(vsomeip3 REQUIRED)`.
  - Marks vsomeip3's include directory as `INTERFACE_SYSTEM_INCLUDE_DIRECTORIES`
    so external-library warnings are suppressed without disabling project
    warnings.
  - Creates a new static library target
    `body_control_lighting_vsomeip_transport` from the five vsomeip sources.
  - Links `vsomeip3` and `body_control_lighting::project_options` to that
    target, scoping the vsomeip3 shared-library dependency to the transport
    library only.
  - Links the vsomeip transport library into `body_control_lighting_core`.

---

### `src/transport/vsomeip/vsomeip_runtime_manager.cpp`

Replaced the UDP-delegation stub with a full vsomeip3 implementation.

**`VsomeipServerAdapterImpl`** (offers a SOME/IP service):

- Constructor accepts app name, service/instance ID, a list of method IDs
  to handle, and a list of `{event_id, eventgroup_id}` pairs to offer.
- `Initialize()` — calls `app->init()`, registers a message handler for
  each method ID, calls `offer_event()` for each event, calls
  `offer_service()`, and starts the vsomeip dispatch loop on a
  `std::thread`.
- `SendResponse()` — looks up the original vsomeip request by
  `{client_id, session_id}` in a mutex-protected `std::map`, calls
  `runtime->create_response()`, sets the payload, and sends.
- `SendEvent()` — creates a payload and calls `app->notify()`.
- Pending requests are stored in a `std::map` protected by `std::mutex`;
  `message_handler_` is stored in a `std::atomic<T*>` so
  `SetMessageHandler()` can remain `noexcept`.

**`VsomeipClientAdapterImpl`** (consumes a SOME/IP service):

- Constructor accepts app name, service/instance ID, and a list of
  `{event_id, eventgroup_id}` pairs to subscribe to.
- `Initialize()` — calls `app->init()`, registers an availability handler,
  calls `request_event()` for each event, registers a single
  `ANY_METHOD` message handler for responses and notifications, calls
  `request_service()`, calls `subscribe()` for each event group, and
  starts the dispatch thread.
- `SendRequest()` — calls `runtime->create_request()`, sets
  service/instance/method/payload, and calls `app->send()`.

**Factory functions** at the bottom of the file:

| Function | Adapter type | App name | Service |
|---|---|---|---|
| `CreateCentralZoneControllerRuntimeAdapter` | Client | `central_zone_controller` | `0x5100` |
| `CreateRearLightingNodeRuntimeAdapter` | Server | `rear_lighting_node_simulator` | `0x5100` |
| `CreateControllerOperatorRuntimeAdapter` | Server | `controller_operator` | `0x5200` |
| `CreateOperatorClientRuntimeAdapter` | Client | `hmi_control_panel` | `0x5200` |

---

### `src/transport/vsomeip/vsomeip_client_adapter.cpp`

No logic change — `CreateCentralZoneControllerVsomeipClientAdapter()` still
forwards to `CreateCentralZoneControllerRuntimeAdapter()`, which now returns
a real vsomeip-backed adapter.

---

### `src/transport/vsomeip/vsomeip_server_adapter.cpp`

No logic change — `CreateRearLightingNodeVsomeipServerAdapter()` still
forwards to `CreateRearLightingNodeRuntimeAdapter()`, which now returns a
real vsomeip-backed adapter.

---

### `config/vsomeip/central_zone_controller.json`

- `"routing": "central_zone_controller"` — this process is the single
  routing master for the loopback simulation.
- Removed the spurious `0x5100` service entry (the controller consumes
  that service; it does not offer it).
- Added `0x5200` (operator service) with `reliable.port: 30520`, events
  `0x8001` and `0x8002`, and eventgroups `0x0001` and `0x0002`.
- Added `controller_operator` (ID `0x1005`) to the `applications` list.

---

### `config/vsomeip/rear_lighting_node_simulator.json`

- Changed `"routing"` from `"rear_lighting_node_simulator"` to
  `"central_zone_controller"` — the simulator connects to the controller's
  routing manager instead of starting its own.
- Kept `0x5100` service entry with `reliable.port: 30510` (the simulator
  offers this service).
- Added `hmi_control_panel` (ID `0x1003`) to the `applications` list.

---

### `src/application/command_arbitrator.cpp`

**Hazard expansion** — a hazard activate or deactivate command now produces
three output commands:

1. `{kHazardLamp, resolved_action}` — updates the hazard lamp status so the
   `ArbitrationContext` reflects the new state.
2. `{kLeftIndicator, resolved_action}` — physically activates/deactivates
   the left indicator.
3. `{kRightIndicator, resolved_action}` — physically activates/deactivates
   the right indicator.

A `kToggle` action is resolved to `kActivate` or `kDeactivate` by checking
`context.hazard_lamp_active`.

**Indicator exclusivity** — activating (or toggling) the left indicator
while `context.right_indicator_active` is true produces two commands:
`[{kRightIndicator, kDeactivate}, {kLeftIndicator, kActivate}]`, result
`kModified`.  The mirror rule applies for right while left is active.

`ArbitrationDecision` was extended in the same phase: the single `command`
field was replaced with `std::array<LampCommand, 3> commands` plus a
`uint8_t command_count`, keeping `Arbitrate()` `noexcept` and avoiding heap
allocation.  `CentralZoneController::SendLampCommand` was updated to iterate
`commands[0..command_count-1]`.

---

### `src/application/lamp_state_manager.cpp`

**Bug fix** — `GetArbitrationContext()` previously only populated
`hazard_lamp_active`; `left_indicator_active` and `right_indicator_active`
were always `false`.  This prevented the indicator exclusivity branches in
`CommandArbitrator` from ever triggering, allowing both indicators to be
active simultaneously.  All five context flags (`left_indicator_active`,
`right_indicator_active`, `hazard_lamp_active`, `park_lamp_active`,
`head_lamp_active`) are now read from the `IsFunctionActive` cache.

---

### `tools/run_simulator.sh`

Exports before launching `rear_lighting_node_simulator`:

```
VSOMEIP_CONFIGURATION=<project_root>/config/vsomeip/rear_lighting_node_simulator.json
VSOMEIP_APPLICATION_NAME=rear_lighting_node_simulator
```

---

### `tools/run_controller.sh`

Exports before launching `central_zone_controller_app`:

```
VSOMEIP_CONFIGURATION=<project_root>/config/vsomeip/central_zone_controller.json
VSOMEIP_APPLICATION_NAME=central_zone_controller
```

---

### `tools/run_hmi.sh`

Fixed wrong config path (`hmi_control_panel.json` → `central_zone_controller.json`).
Exports before launching `hmi_control_panel`:

```
VSOMEIP_CONFIGURATION=<project_root>/config/vsomeip/central_zone_controller.json
VSOMEIP_APPLICATION_NAME=hmi_control_panel
```

---

## Known limitations

- **`VSOMEIP_APPLICATION_NAME` must match the JSON config** — each process
  reads the config file pointed to by `VSOMEIP_CONFIGURATION` and looks up
  its own section by the name in `VSOMEIP_APPLICATION_NAME`.  A mismatch
  causes vsomeip to fall back to default settings and service discovery will
  not work correctly.

- **No separate `routingmanagerd` required** — vsomeip 3 supports an
  embedded routing manager.  `central_zone_controller` is configured as the
  routing master (`"routing": "central_zone_controller"`); other processes
  on the same host connect to it automatically.  Do not run a standalone
  `routingmanagerd` alongside these processes.

- **Tests do not exercise vsomeip** — all nine GoogleTest cases wire
  components through `LoopbackTransportAdapter`, bypassing the vsomeip
  transport entirely.  The vsomeip adapters are exercised only by running
  the three executables end-to-end.

---

## What is unchanged

All nine GoogleTest cases remain green.  The rear lighting service
interface, codec, domain types, operator service layer, and STM32 build
path are unmodified.

# VSS Integration

**COVESA Vehicle Signal Specification (VSS) v6.0** is the SDV industry's shared vocabulary
layer — a standardized signal tree that names every measurable or controllable aspect of a
vehicle. This project uses it as a read-side translation layer: the five BCL exterior lamp
functions are mapped to their canonical VSS paths so that any SDV toolchain that speaks
VSS (Kuksa databroker, AWS FleetWise, vehicle HAL) can interpret the output without knowing
BCL-internal types. The mapping is expressed at build time via authored vspec files, a
generated C++ header of path constants, and a thin `VssLampOverlay` adapter class that
converts domain state to a `VssSnapshot` struct at runtime.

---

## Signal mapping

### Standard VSS signals (5 lamp functions → `Vehicle.Body.Lights.*`)

| BCL `LampFunction` | Standard VSS path | Notes |
|---|---|---|
| `kLeftIndicator` | `Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling` | Exact match |
| `kRightIndicator` | `Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling` | Exact match |
| `kHazardLamp` | `Vehicle.Body.Lights.Hazard.IsSignaling` | Exact match |
| `kParkLamp` | `Vehicle.Body.Lights.Parking.IsOn` | Exact match |
| `kHeadLamp` | `Vehicle.Body.Lights.Beam.Low.IsOn` | Low-beam only — see headlamp note below |

### BCL extension signals (`Vehicle.Private.BCL.Lighting.*`)

| VSS path | Data type | Description |
|---|---|---|
| `Vehicle.Private.BCL.Lighting.CommandSequenceCounter` | uint16 | Last seq counter echoed by exterior node. Wraps at 65535. |
| `Vehicle.Private.BCL.Lighting.LeftIndicator.ActiveFaultCode` | uint16 | DTC for left indicator. 0x0000 = no fault; 0xB001 = driver fault. |
| `Vehicle.Private.BCL.Lighting.RightIndicator.ActiveFaultCode` | uint16 | DTC for right indicator. 0x0000 = no fault; 0xB002 = driver fault. |
| `Vehicle.Private.BCL.Lighting.Hazard.ActiveFaultCode` | uint16 | DTC for hazard lamp. 0x0000 = no fault; 0xB003 = driver fault. |
| `Vehicle.Private.BCL.Lighting.ParkLamp.ActiveFaultCode` | uint16 | DTC for park lamp. 0x0000 = no fault; 0xB004 = driver fault. |
| `Vehicle.Private.BCL.Lighting.HeadLamp.ActiveFaultCode` | uint16 | DTC for headlamp. 0x0000 = no fault; 0xB005 = driver fault. |

Total signals in the runtime snapshot: **11** (5 standard booleans + 6 BCL uint16 extensions).

---

## Headlamp mapping decision

> Pulled verbatim from `vss/spec/bcl_lighting.vspec` header:
>
> The project domain models `kHeadLamp` as a single generic on/off signal.
> VSS v6.0 provides only `Beam.Low.IsOn` and `Beam.High.IsOn`; there is no
> generic "headlamp" signal. We map `kHeadLamp` to `Beam.Low.IsOn` because
> low-beam is the standard dipped/daytime configuration that corresponds
> to what a driver means by "headlamps on". This is a documentation
> decision, not an assertion of canonical equivalence; OEM implementations
> would split the domain into distinct low-beam and high-beam functions.
> `Vehicle.Body.Lights.Beam.High.IsOn` is intentionally absent from this
> overlay: the project domain does not model high-beam, so emitting a
> constant false would mislead signal consumers about live vehicle state.

---

## Why `IsDefect` is declared but not published

The vspec files declare `IsDefect` signals for all five lamp functions
(e.g. `Vehicle.Body.Lights.DirectionIndicator.Left.IsDefect`) to keep the BCL overlay
complete relative to the standard VSS `Body.Lights` signal subset — every signal that
VSS defines for a lamp function is present in the spec. At runtime, however, the
`VssLampOverlay` publishes only the 11 keys listed above. `IsDefect` is not included
because the BCL domain tracks faults as DTC codes in `LampFaultStatus`, not as per-lamp
boolean defect flags; publishing `IsDefect` would require a separate mapping layer and
would reduce precision (one bool vs. one uint16 DTC). The BCL extension
`ActiveFaultCode` signals convey the same information at higher fidelity.

---

## Why `Vehicle.Private.BCL.Lighting.*` and not `Vehicle.Body.Lights.BCL.*`

VSS v6.0 convention: `Vehicle.Private.*` is the reserved namespace for OEM and vendor
signals that have no equivalent in the standard catalog. Placing the BCL extension
signals here (rather than splicing a `BCL` branch into `Vehicle.Body.Lights.*`) keeps
the standard catalog branch clean. A signal consumer that reads
`Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling` knows it is reading a
standard COVESA-defined signal; a consumer that reads
`Vehicle.Private.BCL.Lighting.LeftIndicator.ActiveFaultCode` knows immediately that
this is vendor-specific. Mixing the two by inventing
`Vehicle.Body.Lights.BCL.LeftIndicator.ActiveFaultCode` would violate this convention
and break toolchains that validate signal paths against the upstream catalog schema.

---

## CommandSequenceCounter source

The `CommandSequenceCounter` VSS field is taken from the first non-`kUnknown` entry
in the `LampStatus` array passed to `VssLampOverlay::Snapshot`. This is a deliberate
simplification — production systems would expose a single canonical counter from the
controller — but it is adequate for a portfolio-scale demonstration and the choice is
documented at the source level in `src/vss/vss_lamp_overlay.hpp`.

---

## JSON output format

The `--vss-snapshot` output is human-readable pretty-printed JSON (multi-line, 2-space
indent). For pipe-friendly single-line output, use:

```
./build/app/diagnostic_console --vss-snapshot | jq -c .
```

Pretty format was chosen because the typical use is one-shot diagnostic display to an
engineer, where readability outweighs pipe-friendliness; `jq` handles either format
transparently.

---

## Test scope clarification

The unit tests in `test/test_vss_lamp_overlay.cpp` exercise the overlay contract
(lookup by `LampFunction` name) in isolation, not full-system behavior during e.g. a
hazard-on event. During real hazards, the controller drives both indicator outputs in
addition to the hazard output; integration verification of that scenario is covered by
existing system tests, not by the overlay unit tests.

---

## Build integration

The VSS generation pipeline is controlled by the CMake cache variable
`BODY_CONTROL_LIGHTING_BUILD_VSS` with three states:

| Value | Behavior |
|---|---|
| `AUTO` (default) | Probe for `vspec` on `PATH`. Enable if found; degrade gracefully with a status message if absent. |
| `ON` | Require `vspec`; abort configure with `FATAL_ERROR` if not found. Use in CI. |
| `OFF` | Skip entirely. No probe, no targets, no generated header. |

### Enabling VSS locally

```sh
pip install -r vss/requirements.txt      # installs vss-tools==6.0.0
cmake -S . -B build -DBODY_CONTROL_LIGHTING_BUILD_VSS=ON
cmake --build build -j$(nproc)
```

The generated header is written to `build/include/vss/bcl_vss_paths.hpp` and is
covered by the top-level `.gitignore` entry for `build/`. It is never committed.

---

## How to read a snapshot

With `central_zone_controller_app` and `exterior_lighting_node_simulator` running:

> **Note on piping to jq:** The diagnostic_console binary depends on vsomeip,
> which writes startup logs (DLT messages) to stdout before the JSON output is
> produced. As a result, raw `--vss-snapshot | jq` pipes will fail with a parse
> error on the leading log lines. To extract just the JSON for piping, use
> `awk` to bracket the JSON block:
>
> ```
> ./build/app/diagnostic_console --vss-snapshot | awk '/^{/,/^}/' | jq .
> ```
>
> This is vsomeip middleware behavior, not specific to the VSS integration.
> A cleaner future fix is to route vsomeip logs to stderr via vsomeip
> configuration; that change is out of scope for the current iteration.

```sh
# Human-readable (default)
./build/app/diagnostic_console --vss-snapshot

# Single line for piping
./build/app/diagnostic_console --vss-snapshot | awk '/^{/,/^}/' | jq -c .

# Verify key count
./build/app/diagnostic_console --vss-snapshot | awk '/^{/,/^}/' | jq 'keys | length'
# Expected: 11

# Extract a specific signal
./build/app/diagnostic_console --vss-snapshot | awk '/^{/,/^}/' | \
  jq '."Vehicle.Body.Lights.Hazard.IsSignaling"'
```

Exit codes:

| Code | Meaning |
|---|---|
| 0 | Success; JSON on stdout |
| 2 | VSS integration not built (rebuild with `BODY_CONTROL_LIGHTING_BUILD_VSS=ON`) |
| 3 | Controller or exterior node unreachable within timeout |

**Note on per-function DTC codes:** via the operator service path used by
`--vss-snapshot`, all `ActiveFaultCode` fields will be `0` because
`NodeHealthStatus` reports only aggregate fault count, not per-function DTC codes.
For per-function DTCs (0xB001–0xB005), use the UDS/DoIP path:

```sh
python3 tools/uds_client/uds_client.py --service 0x19 --subfunc 0x02 --dtc-mask 0xB0
```

---

## What is out of scope

The following are explicitly not part of this integration:

- **Kuksa databroker** — no runtime pub/sub infrastructure, no gRPC, no databroker
  process. The VSS paths are used as string constants and output labels only.
- **gRPC / proto service definitions** — not implemented; out of budget for a
  portfolio-scale demonstration.
- **Vehicle.* coverage beyond Body.Lights** — `Vehicle.Powertrain`, `Vehicle.ADAS`,
  etc. are unrelated to the zonal lighting scope.
- **Overlay updates beyond DTCs** — the `VssLampOverlay` does not handle
  `IsDefect` at runtime, predictive signals, or adaptive lighting states.
- **Signal history / time series** — the snapshot is a point-in-time read; no
  logging, buffering, or replay.

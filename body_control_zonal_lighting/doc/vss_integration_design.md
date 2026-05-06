# VSS Integration Design

**Document scope:** vocabulary mapping, overlay vspec authoring, generated C++ header
strategy, `VssLampOverlay` adapter design, and `--vss-snapshot` diagnostic extension.
This document does **not** cover Kuksa databroker, gRPC servers, or AWS FleetWise.

---

## 1. Current State of VSS

### 1.1 Latest releases

| Component | Version | Released |
|---|---|---|
| vehicle_signal_specification | **v6.0** | 2026-01-16 |
| vss-tools | No formal tagged release on GitHub; install via `pip install vss-tools` from master |

VSS v6.0 notable changes relevant to this project: `Vehicle.OBD` branch removed,
string pattern keywords added, units normalised (`Celsius` capitalised),
Vehicle Health Management signals added.

### 1.2 vss-tools exporters (from `vspec export --help`, master branch)

| Exporter name | Output format |
|---|---|
| `json` | JSON (flat or nested) |
| `yaml` | YAML |
| `csv` | CSV |
| `protobuf` | Protocol Buffers `.proto` |
| `jsonschema` | JSON Schema |
| `ddsidl` | DDS-IDL |
| `binary` | Binary blob |
| `plantuml` | PlantUML diagram |
| `franca` | Franca IDL |
| `id` | Signal ID list |
| `apigear` | ApiGear |
| `samm` | ESMF/SAMM Turtle (`.ttl`) |
| `tree` | ASCII tree (inspection only) |
| `ros2interface` | ROS2 message/service files |

CLI pattern: `vspec export json --vspec spec/VehicleSignalSpecification.vspec --output out.json`

### 1.3 `Vehicle.Body.Lights` hierarchy (VSS v6.0)

The Body.vspec file composes the Lights branch from three shared templates:

- **`StaticLights.vspec`** — provides `IsOn` (actuator, boolean) + `IsDefect` (sensor, boolean)
- **`SignalingLights.vspec`** — provides `IsSignaling` (actuator, boolean) + `IsDefect` (sensor, boolean)
- **`BrakeLights.vspec`** — provides `IsActive` (actuator, boolean) + `IsDefect` (sensor, boolean)

Full expanded signal table for the five lamp functions in scope:

| VSS path | Type | Data type | Description |
|---|---|---|---|
| `Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling` | actuator | boolean | Left direction indicator active. True = signaling. |
| `Vehicle.Body.Lights.DirectionIndicator.Left.IsDefect` | sensor | boolean | Left indicator circuit defect detected. |
| `Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling` | actuator | boolean | Right direction indicator active. |
| `Vehicle.Body.Lights.DirectionIndicator.Right.IsDefect` | sensor | boolean | Right indicator circuit defect detected. |
| `Vehicle.Body.Lights.Hazard.IsSignaling` | actuator | boolean | Hazard warning active (all four corners). |
| `Vehicle.Body.Lights.Hazard.IsDefect` | sensor | boolean | Hazard circuit defect detected. |
| `Vehicle.Body.Lights.Parking.IsOn` | actuator | boolean | Parking/side-marker lamp on. |
| `Vehicle.Body.Lights.Parking.IsDefect` | sensor | boolean | Parking lamp circuit defect detected. |
| `Vehicle.Body.Lights.Beam.Low.IsOn` | actuator | boolean | Low-beam headlamp on. |
| `Vehicle.Body.Lights.Beam.Low.IsDefect` | sensor | boolean | Low-beam circuit defect detected. |
| `Vehicle.Body.Lights.Beam.High.IsOn` | actuator | boolean | High-beam headlamp on. |
| `Vehicle.Body.Lights.Beam.High.IsDefect` | sensor | boolean | High-beam circuit defect detected. |
| `Vehicle.Body.Lights.LightSwitch` | actuator | string | Master light switch position. |
| `Vehicle.Body.Lights.IsHighBeamSwitchOn` | actuator | boolean | High-beam switch engaged. |

Standard branches not mapped: `Lights.Running`, `Lights.Backup`,
`Lights.Fog.*`, `Lights.Brake`, `Lights.LicensePlate`.

### 1.4 Lamp-to-signal mapping

| BCL `LampFunction` | Standard VSS path | Match quality | Notes |
|---|---|---|---|
| `kLeftIndicator` | `Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling` | **Exact** | — |
| `kRightIndicator` | `Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling` | **Exact** | — |
| `kHazardLamp` | `Vehicle.Body.Lights.Hazard.IsSignaling` | **Exact** | — |
| `kParkLamp` | `Vehicle.Body.Lights.Parking.IsOn` | **Exact** | — |
| `kHeadLamp` | `Vehicle.Body.Lights.Beam.Low.IsOn` | **Exact — low beam** | BCL `kHeadLamp` maps to low-beam only; high-beam not wired in this project |

All five lamp functions map cleanly to standard VSS paths. No BCL-prefixed extension
branch is required for the command/state signals. A narrow overlay is still authored
(`Vehicle.Body.Lights.BCL.*`) to carry BCL-specific metadata (fault DTC codes,
sequence counter) that have no standard VSS equivalent.

#### Overlay extension: `Vehicle.Body.Lights.BCL`

| VSS path (overlay) | Type | Data type | Description |
|---|---|---|---|
| `Vehicle.Body.Lights.BCL.LeftIndicator.ActiveFaultCode` | sensor | uint16 | Active DTC for left indicator (0xB001 or 0x0000). |
| `Vehicle.Body.Lights.BCL.RightIndicator.ActiveFaultCode` | sensor | uint16 | Active DTC for right indicator (0xB002 or 0x0000). |
| `Vehicle.Body.Lights.BCL.Hazard.ActiveFaultCode` | sensor | uint16 | Active DTC for hazard lamp (0xB003 or 0x0000). |
| `Vehicle.Body.Lights.BCL.ParkLamp.ActiveFaultCode` | sensor | uint16 | Active DTC for park lamp (0xB004 or 0x0000). |
| `Vehicle.Body.Lights.BCL.HeadLamp.ActiveFaultCode` | sensor | uint16 | Active DTC for headlamp (0xB005 or 0x0000). |
| `Vehicle.Body.Lights.BCL.CommandSequenceCounter` | sensor | uint16 | Last command sequence counter echoed by the exterior node. |

---

## 2. Scope Proposal

**Scope:** vocabulary alignment (vspec authoring), build-time code generation
(`vspec export json` → generated C++ header), a thin `VssLampOverlay` adapter
class, and a `--vss-snapshot` flag on the diagnostic console.
This is **not** a runtime databroker or telemetry pipeline.

### In scope

- Authoring `vss/spec/Vehicle.Body.Lights.BCL.vspec` overlay extension
- Pinned vss-tools invocation in CMake to emit `vss_body_lights.hpp` at configure time
- `body_control::lighting::vss::VssLampOverlay` adapter class (no transport dependency)
- Unit tests for all five lamp functions and the JSON serialiser
- `--vss-snapshot` CLI flag on `app/diagnostic_console/main.cpp`
- `doc/vss_integration.md` user-facing explanation
- README Skills Demonstrated row update

### Out of scope

- **Kuksa databroker** — runtime pub/sub infrastructure; out of budget and not needed for portfolio demonstration.
- **gRPC server** — requires proto service definitions and generated stubs; orthogonal to this design.
- **AWS FleetWise** — cloud telemetry pipeline; requires cloud credentials and SDK; no value for an embedded portfolio project.
- **VSS branches beyond Body.Lights** — `Vehicle.Powertrain`, `Vehicle.ADAS`, etc. are unrelated to the zonal lighting scope.

---

## 3. Architecture

### 3a. Folder layout

```
vss/
  spec/
    Vehicle.Body.Lights.BCL.vspec     # BCL overlay extension (custom signals)
  generated/
    vss_body_lights.hpp               # generated at build — gitignored
src/
  vss/
    vss_lamp_overlay.hpp
    vss_lamp_overlay.cpp
test/
  unit/
    test_vss_lamp_overlay.cpp
```

**Generated-file policy:** `vss/generated/` is listed in `.gitignore`.
Rationale: the generated header is fully reproducible from the pinned vss-tools
version and the vspec sources checked into the repo. Committing generated artifacts
introduces noise into diffs, risks stale files when the spec changes, and adds
binary-like churn to code review. The CMake custom target regenerates the header
during cmake configure when Python and vss-tools are present; if absent, the build
falls back gracefully with a warning.

### 3b. Toolchain decision

**Pin:** `vss-tools` installed from PyPI at `pip install vss-tools==5.0.0` (or the
latest stable release available at integration time; record exact version in
`vss/requirements.txt`).

**Two approaches compared:**

| Approach | Pros | Cons |
|---|---|---|
| **JSON-at-runtime** — ship `vss.json`, parse at startup | No Python at build time; signal names inspectable at runtime | Runtime parse cost; JSON lib dependency; signals not type-checked at compile time |
| **Generated C++ header at configure time** — Python runs once, emits a `.hpp` with `constexpr std::string_view` constants | Type-safe at compile time; zero runtime cost; no extra runtime dependency; CI reproducible | Python + vss-tools required at configure time |

**Recommendation: generated C++ header.**
The project already targets -Wall -Wextra -Wpedantic and has zero runtime
dependencies in the embedded targets. A generated header of `constexpr std::string_view`
path constants is checked at compile time, incurs no runtime parse cost, and fits
the existing code style. The Python dependency is gated behind
`BODY_CONTROL_LIGHTING_BUILD_VSS=ON` and never a hard build failure.

### 3c. Build integration

```cmake
option(BODY_CONTROL_LIGHTING_BUILD_VSS
       "Generate VSS header and build VssLampOverlay adapter" OFF)

if(BODY_CONTROL_LIGHTING_BUILD_VSS)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(NOT Python3_FOUND)
        message(WARNING "BODY_CONTROL_LIGHTING_BUILD_VSS=ON but Python3 not found. "
                        "VSS generation skipped.")
    else()
        execute_process(COMMAND ${Python3_EXECUTABLE} -m vss_tools --version
                        RESULT_VARIABLE _vss_tools_result OUTPUT_QUIET ERROR_QUIET)
        if(NOT _vss_tools_result EQUAL 0)
            message(WARNING "vss-tools not found (pip install vss-tools==5.0.0). "
                            "VSS generation skipped.")
        else()
            set(VSS_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vss/generated")
            set(VSS_SPEC_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/vss/spec")

            add_custom_target(vss_generate
                COMMAND ${Python3_EXECUTABLE} -m vss_tools export json
                    --vspec "${VSS_SPEC_DIR}/Vehicle.Body.Lights.BCL.vspec"
                    --output "${VSS_GENERATED_DIR}/vss_body_lights.json" --pretty
                COMMAND ${CMAKE_COMMAND} -P
                    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateVssHeader.cmake"
                COMMENT "Generating VSS header from vspec" VERBATIM)

            add_library(body_control_vss STATIC src/vss/vss_lamp_overlay.cpp)
            target_include_directories(body_control_vss PUBLIC
                include "${VSS_GENERATED_DIR}")
            target_link_libraries(body_control_vss PUBLIC body_control_lighting::core)
            add_dependencies(body_control_vss vss_generate)

            target_include_directories(body_control_lighting::core INTERFACE
                $<$<TARGET_EXISTS:body_control_vss>:${VSS_GENERATED_DIR}>)
        endif()
    endif()
endif()
```

Key constraints: `BODY_CONTROL_LIGHTING_BUILD_VSS` defaults **OFF** — existing builds
are unaffected. Missing Python or vss-tools emits `message(WARNING ...)` and skips
the target — never `FATAL_ERROR`. CI adds one extra step only in the VSS job:
`pip install vss-tools==5.0.0`.

### 3d. VssLampOverlay adapter design

The adapter sits in the **application layer** — it reads from `LampStateManager`
(application) and produces a VSS-typed snapshot. It has no transport dependency
and no vsomeip dependency; it is testable in isolation with GoogleTest.

```cpp
namespace body_control::lighting::vss {

// Field names mirror the last VSS path element.
struct VssLightSignals {
    bool is_signaling_left  {false};   // DirectionIndicator.Left.IsSignaling
    bool is_signaling_right {false};   // DirectionIndicator.Right.IsSignaling
    bool is_hazard_on       {false};   // Hazard.IsSignaling
    bool is_parking_on      {false};   // Parking.IsOn
    bool is_beam_low_on     {false};   // Beam.Low.IsOn
    std::uint16_t dtc_left_indicator  {0U};  // BCL.LeftIndicator.ActiveFaultCode
    std::uint16_t dtc_right_indicator {0U};  // BCL.RightIndicator.ActiveFaultCode
    std::uint16_t dtc_hazard          {0U};  // BCL.Hazard.ActiveFaultCode
    std::uint16_t dtc_park_lamp       {0U};  // BCL.ParkLamp.ActiveFaultCode
    std::uint16_t dtc_headlamp        {0U};  // BCL.HeadLamp.ActiveFaultCode
    std::uint16_t command_seq_counter {0U};  // BCL.CommandSequenceCounter
};

// No transport dependency. No vsomeip dependency. Unit-testable in isolation.
class VssLampOverlay {
public:
    VssLampOverlay() noexcept = default;

    // Maps LampStatus array + fault table + seq_counter to VssLightSignals.
    [[nodiscard]] VssLightSignals Translate(
        const std::array<domain::LampStatus, 5U>& lamp_statuses,
        const domain::LampFaultStatus& fault_status,
        std::uint16_t seq_counter) const noexcept;

    // Flat JSON object keyed by full VSS path strings. Allocates; Linux only.
    [[nodiscard]] std::string ToJson(const VssLightSignals& signals) const;
};

} // namespace body_control::lighting::vss
```

**Placement in the layer stack:**

```
app/diagnostic_console  ─┐
                          │  calls VssLampOverlay::Translate() + ToJson()
                          ▼
                    vss::VssLampOverlay          ← NEW (application layer peer)
                          │
                          │  reads domain types only
                          ▼
              domain::LampStatus, domain::LampFaultStatus
```

`VssLampOverlay` is a peer of the `application` layer classes. It depends on
`domain` types only — the same constraint applied to `CommandArbitrator` and
`LampStateManager`. It is never included by `service/` or `transport/`.

---

## 4. Integration Points

### 4a. Layer placement

Confirmed from reading `include/body_control/lighting/` and `src/`:

```
domain  →  application (VssLampOverlay peer)  →  service  →  transport  →  platform
```

`VssLampOverlay` is instantiated by `app/diagnostic_console/main.cpp` alongside
`OperatorServiceConsumer`. It reads `LampStatus` and `LampFaultStatus` values
obtained from the service consumer's cache — it does not subscribe to events and
does not modify any existing call path.

### 4b. Diagnostic console `--vss-snapshot` flag

When `./diagnostic_console --vss-snapshot` is passed:

1. The console connects to the operator service as normal.
2. After the service reports availability, it reads the cached lamp statuses for
   all five functions and the cached fault status.
3. `VssLampOverlay::Translate()` maps them to `VssLightSignals`.
4. `VssLampOverlay::ToJson()` serialises to stdout and the process exits with 0.

No interactive menu is shown. The flag is mutually exclusive with interactive mode.
Intended use: scripted regression checks, CI smoke tests, log capture.

Example output:
```json
{
  "Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling": false,
  "Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling": false,
  "Vehicle.Body.Lights.Hazard.IsSignaling": false,
  "Vehicle.Body.Lights.Parking.IsOn": true,
  "Vehicle.Body.Lights.Beam.Low.IsOn": true,
  "Vehicle.Body.Lights.BCL.LeftIndicator.ActiveFaultCode": 0,
  "Vehicle.Body.Lights.BCL.RightIndicator.ActiveFaultCode": 0,
  "Vehicle.Body.Lights.BCL.Hazard.ActiveFaultCode": 0,
  "Vehicle.Body.Lights.BCL.ParkLamp.ActiveFaultCode": 0,
  "Vehicle.Body.Lights.BCL.HeadLamp.ActiveFaultCode": 0,
  "Vehicle.Body.Lights.BCL.CommandSequenceCounter": 7
}
```

### 4c. Qt HMI

**Recommendation: do not display VSS paths in the Qt HMI.**
The HMI is operator-facing and uses human-readable labels defined in
`hmi_display_strings.hpp`. Surfacing VSS path strings (`Vehicle.Body.Lights...`)
in a vehicle HMI would be a developer artefact, not a user-facing concept.
VSS output is restricted to the diagnostic console.

### 4d. Wire protocol unchanged

The following are explicitly unchanged by this integration:

- SOME/IP service IDs: `0x5100` (ExteriorLightingService), `0x5200` (OperatorService)
- SOME/IP method IDs, event IDs, event group IDs: unchanged
- UDP ports: 41000, 41001, 41002, 41003: unchanged
- UDS over DoIP TCP 13400: unchanged
- UDS DIDs: `0xF190` (VIN), `0xF18C` (ECU serial), `0xF101` (SW version): unchanged
- `LampCommand` and `LampStatus` wire encoding (8-byte fixed payload): unchanged
- FaultCode values (`0xB001`–`0xB005`): unchanged

`VssLampOverlay` is a read-only observer; it never writes to the transport.

---

## 5. Deliverables and Acceptance Criteria

```
NEW  vss/spec/Vehicle.Body.Lights.BCL.vspec
NEW  vss/generated/vss_body_lights.hpp          (gitignored; generated at configure)
NEW  vss/requirements.txt                       (pins vss-tools version)
NEW  src/vss/vss_lamp_overlay.hpp
NEW  src/vss/vss_lamp_overlay.cpp
NEW  test/unit/test_vss_lamp_overlay.cpp
MOD  app/diagnostic_console/main.cpp            (--vss-snapshot flag)
MOD  CMakeLists.txt                             (BODY_CONTROL_LIGHTING_BUILD_VSS option)
MOD  .gitignore                                 (vss/generated/)
NEW  doc/vss_integration.md                     (user-facing explanation)
MOD  README.md                                  (Skills Demonstrated row)
```

**Unit tests** (`test_vss_lamp_overlay.cpp`): 7 tests expected.

| Test | What it verifies |
|---|---|
| `TranslateLeftIndicatorOn` | `kLeftIndicator` kOn → `is_signaling_left = true` |
| `TranslateRightIndicatorOn` | `kRightIndicator` kOn → `is_signaling_right = true` |
| `TranslateHazardOn` | `kHazardLamp` kOn → `is_hazard_on = true` |
| `TranslateParkLampOn` | `kParkLamp` kOn → `is_parking_on = true` |
| `TranslateHeadLampOn` | `kHeadLamp` kOn → `is_beam_low_on = true` |
| `TranslateActiveFaultCode` | `FaultCode::kParkLamp` active → `dtc_park_lamp = 0xB004` |
| `ToJsonFormatCheck` | JSON output contains all 11 expected keys with correct types |

All **14 existing unit tests** (pre-VSS integration) must keep passing.
`BODY_CONTROL_LIGHTING_BUILD_VSS=OFF` (default) must produce a clean build with
no new targets and no warnings introduced.

**README.md Skills Demonstrated row:**

```markdown
| **VSS (COVESA)** | Signal vocabulary mapping for 5 lamp functions to `Vehicle.Body.Lights.*`; BCL overlay vspec; build-time C++ header generation via vss-tools; `VssLampOverlay` adapter; `--vss-snapshot` diagnostic CLI flag |
```

---

## 6. Risks and Mitigations

| Risk | Mitigation |
|---|---|
| vss-tools Python version mismatch on Ubuntu 24.04 / GH Actions | Pin exact version in `vss/requirements.txt`; CI step installs into a venv before invoking CMake with `-DBODY_CONTROL_LIGHTING_BUILD_VSS=ON` |
| VSS catalog drift — upstream renames signals in v7.x | Lock spec at tagged VSS version; document refresh policy: re-verify signal paths on each VSS major release; overlay extension provides a stable BCL-namespaced alias if standard paths change |
| Name collisions with existing domain types | All VSS adapter code lives in `namespace body_control::lighting::vss`; generated header in `vss/generated/` never pollutes the main include tree |
| cppcheck / clang-tidy running on generated code | Add `vss/generated/*` to `.cppcheckrc` suppressions and exclude from the `find ... -name "*.hpp"` clang-tidy invocation in CI |
| `BODY_CONTROL_LIGHTING_BUILD_VSS=ON` breaks default build on embedded targets (STM32, Zephyr) | Option defaults `OFF`; embedded platform CMake presets never set it; `ToJson()` uses `std::string` which is excluded from the embedded translation units by design |

---

## 7. Effort Breakdown

| Subtask | Hours | Risk |
|---|---|---|
| vspec authoring + overlay extension | 0.5 h | Low |
| **CMake build integration** | **1.5 h** | **Medium — Python/vss-tools version on CI is the riskiest item** |
| VssLampOverlay class + JSON serialiser | 1.5 h | Low |
| Unit tests (7 cases) | 1.0 h | Low |
| Diagnostic console `--vss-snapshot` flag | 0.5 h | Low |
| Integration testing + static analysis clean-up | 0.5 h | Low |
| Docs (`vss_integration.md`, README row) | 0.5 h | Low |
| **Total** | **~6 h** | |

**Riskiest item:** CMake build integration. The custom target must locate Python,
invoke vss-tools, and invoke a helper CMake script to convert JSON to a C++ header,
all while degrading gracefully if any step fails. This has the most moving parts
and the most variation across developer machines and CI images.

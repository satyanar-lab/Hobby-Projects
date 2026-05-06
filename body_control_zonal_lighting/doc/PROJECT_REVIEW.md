# Status — P1 follow-up

The P1 findings from section 13 have all been addressed in follow-up commits.
P2 items remain open and tracked in the same section.

| # | Item | Status | Commit |
|---|---|---|---|
| P1 #1 | HealthThread hard-coded fault state (Zephyr) | Fixed and verified on hardware | f9218c8 |
| P1 #2 | GPIO pin comment header vs DTS discrepancy | Fixed (comment-only, DTS unchanged) | 0a9ff73 |
| P1 #3 | FMEA stale OTA file/method references | Fixed (broader sweep also caught traceability map drift) | b2e7b6a |
| P1 #4 | VSS design doc namespace and struct name | Fixed (broader sweep also corrected method signature, vspec filename, field names, test table) | c6c31cd |
| P1 #5 | README test count and missing roadmap phases | Fixed (count corrected to 15; Phases 13, 14, 15 added) | 157ccb7 |

## P1 #1 hardware verification

The HealthThread fix was verified end-to-end on the NUCLEO-H753ZI at
192.168.0.20. Three rounds of NodeHealthEvent capture (~5 seconds each)
bracketing UDS fault inject and clear:

    Baseline (no fault):
      payload bytes:       01 01 01 00 00 00
      health_state:        0x01 (kOperational)
      fault_present:       0
      active_fault_count:  0

    After UDS fault inject (left_indicator, DTC 0xB001):
      payload bytes:       03 01 01 01 00 01
      health_state:        0x03 (kFaulted)
      fault_present:       1
      active_fault_count:  1

    After UDS fault clear:
      payload bytes:       01 01 01 00 00 00
      (reverts to baseline)

The SOME/IP NodeHealthEvent now carries live fault state from FaultManager.
The pre-fix hard-coded false/0 fiction is gone; the event tracks UDS fault
inject and clear in real time.

---

# Body Control Zonal Lighting — Project Review

*End-to-end self-review against senior-engineer review standards. Every
concrete claim cites the source file and line number that justifies it.
Sections are self-contained; cross-section back-references are used where
the same artifact is relevant to multiple judgements.*

---

## 1  Executive Summary

This is a portfolio-grade embedded C++ project that demonstrates genuine
competence in the technologies it targets. The layered architecture is honest,
not cosmetic. The wire protocols are real. The ISO standards it references are
cited correctly and the gaps are documented truthfully. For a candidate
targeting Adaptive AUTOSAR / Ethernet / SDV integration engineering roles, this
project gives an interviewer concrete evidence to examine rather than bullet
points to probe.

**Verdict on portfolio readiness.** The project is ready to be shown as primary
portfolio evidence for mid-level and senior roles in automotive software
integration. It will pass a first-round technical screen and give a hiring
manager enough surface area to run a 90-minute technical conversation. It is not
ready to be claimed as production-quality firmware — the project itself says so,
and the honesty is a feature, not a defect.

**Role fit.** The project aligns best with: AUTOSAR Integration Engineer
(Ethernet/SOME/IP focus), Software-Defined Vehicle platform engineer, embedded
Linux middleware developer, or automotive BSP / diagnostics engineer. It is
thinner on classic AUTOSAR Classic stack work (Com, Rte, Dcm in the AUTOSAR
Classic sense) and will not cover OSEK/FreeRTOS-centric ECU roles.

**Top three strengths.**

1. *Layering is enforced, not aspirational.* The CMake build separates domain,
   application, service, transport, and platform into distinct static libraries
   with explicit dependency edges. Domain types carry no transport or platform
   headers. The `TransportAdapterInterface`
   (`include/body_control/lighting/transport/transport_adapter_interface.hpp`)
   is a genuine seam that lets the same application code compile for Linux
   vsomeip, STM32 LwIP, and Zephyr BSD sockets without `#ifdef` in the logic
   layers.

2. *Protocol implementation is traceable to the specification.* The DoIP server
   (`src/transport/doip_server.cpp:129`) validates protocol version 0xFD and
   handles payload type routing (0x0005, 0x0005, 0x8001, 0x8002) correctly. The
   SOME/IP-SD codec (`src/transport/some_ip_sd/some_ip_sd_codec.cpp:45`)
   documents the non-obvious `kOptContentLen = 0x0009` derivation from the
   AUTOSAR AP_PRS_SOMEIPServiceDiscovery specification. UDS NRC codes map to the
   correct ISO 14229-1 values (see `src/application/uds_request_handler.cpp`).

3. *Security and safety gaps are documented proactively.* Rather than silently
   using a test key, `doc/security_architecture.md` names the MCUboot test key
   as gap 5.4 with a production fix path. `safety/05_fmea.md` lists three open
   RPN items (FM-002 GPIO stuck-high 120, FM-010 absent IWDG 112, FM-005
   network flood 96) with explicit "Gap" labels. This is the posture of an
   engineer who has done this for real.

**Top three concerns.**

1. *Health event propagation is broken on Zephyr.* `HealthThread` in
   `app/zephyr_nucleo_h753zi/main.cpp:765–769` hard-codes
   `health.lamp_driver_fault_present = false` and `health.active_fault_count =
   0U` unconditionally. `FaultManager::PopulateHealth()` is never called on the
   health-publish path. Fault injection over UDS works (DTCs are stored in
   `g_lamp_mgr`), but the health event visible to the CZC and HMI always reports
   the node as fault-free. This is a functional correctness bug, not a style
   concern.

2. *CRC validation on OTA transfer exit is optional on the server side.* The
   server (`src/application/ota_session_manager.cpp:184`) validates the CRC
   only `if (req.size() == 5U)`. A client that sends a 1-byte 0x37 (SID only)
   bypasses CRC validation entirely and the transfer succeeds. The Python client
   always sends the CRC (see `tools/ota_client/ota_client.py`), but a
   malicious or buggy tester can skip it.

3. *Zephyr CI is absent.* No GitHub Actions job builds or tests the Zephyr
   target. The overlay file
   (`app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay:23`) references
   `gpioa 5` for `left_indicator`, while the file's own comment header (line
   14) says `PB5 → Left Indicator`. That discrepancy exists in the repository
   undetected because there is no Zephyr build in CI.

---

## 2  Architecture Review

### Layer separation

The project declares five layers and enforces them through the build system:
`domain`, `application`, `service`, `transport`, `platform`. The CMake target
hierarchy mirrors this.

`domain` types (`include/body_control/lighting/domain/`) carry no standard
library containers beyond `std::array` and `std::uint*_t` scalars, no includes
from other layers, and no platform headers. All five layers depend downward
only — domain knows nothing, application depends only on domain, service depends
on application and transport interfaces, platform code is compiled in only when
the corresponding `BODY_CONTROL_LIGHTING_TARGET_PLATFORM` is selected. This is
verifiable: `grep -r '#include "body_control/lighting/transport'
include/body_control/lighting/domain/` returns nothing.

The `TransportAdapterInterface`
(`include/body_control/lighting/transport/transport_adapter_interface.hpp`) is
the architectural seam that makes multi-platform compilation possible without
`#ifdef`. The same `ExteriorLightingServiceProvider` runs over the vsomeip
adapter on Linux, the LwIP adapter on STM32 bare-metal, and the Zephyr UDP
adapter on Zephyr. This is the single most important architectural decision in
the project, and it is implemented correctly.

### Service/transport split

The custom SOME/IP-shaped transport uses a 20-byte header for service data and a
standard 16-byte SOME/IP header for SOME/IP-SD traffic. This duality is
documented in `src/transport/some_ip_sd/some_ip_sd_codec.cpp:1–10`. The split
is architecturally sound: the SD codec is separate from the service-data message
builder/parser (`src/transport/someip_message_builder.cpp`,
`src/transport/someip_message_parser.cpp`), and the service layer does not need
to know which codec is in use.

The `TransportMessage` struct (in
`include/body_control/lighting/transport/transport_adapter_interface.hpp`) is
the canonical inter-layer data type. It carries `service_id`, `instance_id`,
`method_or_event_id`, `client_id`, `session_id`, `is_event`, `is_reliable`, and
a `payload` vector. The use of a `std::vector<uint8_t>` for payload means every
message allocation on the Linux path allocates heap memory. This is fine for a
Linux simulator; the embedded paths avoid it by never going through
`TransportMessage` directly — Zephyr and STM32 handle their own framing inside
their respective transport adapters.

### Cross-platform abstraction

The CMake `BODY_CONTROL_LIGHTING_TARGET_PLATFORM` option selects platform
sources at configure time. `src/CMakeLists.txt` maintains a separate
`BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32` list that includes
`ota_session_manager_stm32.cpp` (stub) instead of the Linux POSIX version.
Zephyr builds use the west/sysbuild path and do not go through the CMake
`TARGET_PLATFORM` mechanism.

One gap: the new `src/application/ota_session_manager_stm32.cpp` (present as
untracked in git status) is not yet wired into `src/CMakeLists.txt`'s
`BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32` list if it was intended to replace
the existing stub. Verify this before the next STM32 bare-metal build.

### Adaptive AUTOSAR claims

The ARXML descriptions in `arxml/` follow the AUTOSAR R20-11 schema, and the
README mapping table in `arxml/README.md` traces ARXML elements to C++ classes.
The proxy/skeleton pattern is represented by the consumer/provider facade pairs
in `src/service/`. These are hand-written facades, not generated stubs, which is
appropriate for a portfolio project — and the README accurately says
"proxy/skeleton pattern," not "generated from ARXML." The distinction is
important: a hiring manager familiar with Adaptive AUTOSAR will ask about the
generation step. The honest answer is that this project implements the pattern
manually and uses ARXML for documentation, not for toolchain input.

The SOME/IP-SD implementation follows the AUTOSAR AP_PRS_SOMEIPServiceDiscovery
wire format (session ID increment, flags byte, entries array, options array),
which is verifiable against Wireshark captures in `doc/captures/`.

### Summary of architectural findings

The layering is the strongest technical attribute of this project. The seam at
the transport adapter interface is clean and allows genuine multi-platform
portability. The main architectural debt is the Zephyr `main.cpp` at 1130 lines
— it combines thread entry points, DoIP framing, GPIO setup, SOME/IP-SD
scheduling, and OTA state tracking in a single file with no sub-module
decomposition. This is justified for an embedded firmware main but would not
pass a code review in a production Adaptive AUTOSAR stack.

---

## 3  Code Quality

### Modern C++ discipline

The codebase uses C++17 throughout. The discipline is consistent and the
following patterns are applied correctly across all reviewed files:

- `[[nodiscard]]` on every function returning a status or encoded buffer. Spot-
  check: `LightingPayloadCodec` functions in
  `include/body_control/lighting/domain/lighting_payload_codec.hpp` are all
  `[[nodiscard]]`; `VssLampOverlay::Snapshot()` in
  `src/vss/vss_lamp_overlay.cpp:52` is `[[nodiscard]]`; `CommandArbitrator::
  Arbitrate()` in `include/body_control/lighting/application/command_arbitrator.
  hpp` is `[[nodiscard]] const noexcept`.

- `noexcept` is applied honestly. Leaf functions that genuinely cannot throw
  are marked `noexcept`. The Zephyr `prj.conf` enables
  `CONFIG_CPP_EXCEPTIONS=y` with a rationale comment explaining that
  `SomeipMessageParser` may throw — this means the `noexcept` on upper-layer
  functions that call the parser is only correct if the parser's exceptions are
  caught at the adapter boundary. Verify that the Zephyr UDP transport adapter
  wraps parser calls in a `try/catch` before marking any higher layer
  `noexcept`.

- Scoped enums with explicit underlying types everywhere. `FaultCode` uses
  `std::uint16_t`
  (`include/body_control/lighting/domain/fault_types.hpp`). `LampFunction`,
  `LampOutputState`, `LampCommandAction`, `NodeHealthState` all carry explicit
  underlying types. The `kUnknown = 0` sentinel pattern is applied consistently.

- Default member initializers on all struct fields. `LampStatus {}` zero-
  initializes predictably. `LampFaultStatus {}` initializes `fault_present =
  false`, `active_fault_count = 0U`, `active_faults = {}`.

- No `using namespace` in headers. Confirmed by inspection of all reviewed
  headers.

- `std::array` for fixed-size collections, `std::vector` only where dynamic
  sizing is genuinely needed (UDS responses, encoded frames). The arbitrator
  result uses `std::array<domain::LampCommand, kMaxArbitrationCommands>` with a
  `decision_count` field — correct approach for embedded-friendly fixed
  allocation.

### MISRA-leaning discipline

The project claims "MISRA-oriented" discipline rather than full MISRA C++
compliance, which is accurate. Observed practices:

- No raw owning pointers in public interfaces. Resources are managed by RAII
  (file descriptors in `OtaSessionManager`, sockets in `DoipServer`).

- `static_cast` used consistently for numeric conversions. No C-style casts in
  reviewed sources.

- The `reinterpret_cast<const std::uint8_t*>(kEcuId)` in
  `src/application/uds_request_handler.cpp:389–391` is the single
  `reinterpret_cast` found in the non-platform code. It is technically safe
  (casting from `const char*` to `const uint8_t*` for a static string), but it
  would be flagged by a strict MISRA checker and could be replaced with a
  `std::memcpy` into a `std::vector<uint8_t>` to avoid the cast entirely.

- `SIZE_MAX` as a sentinel from `LampFunctionToIndex` in `fault_manager.cpp` is
  a well-known pattern but carries an implicit contract: every caller must check
  against `faulted_.size()` before using the returned index. All callers do
  check, but the pattern relies on `SIZE_MAX >= faulted_.size()` being obvious
  at every call site. A type-safe `std::optional<std::size_t>` would make the
  contract explicit in the type system.

### Naming conventions

Naming is consistent and automotive-idiomatic throughout:
- Types: `UpperCamelCase`
- Constants: `kUpperCamelCase`
- Member variables: `lower_snake_case_` with trailing underscore
- Non-member functions: `UpperCamelCase` (free functions) or `lower_snake_case`
  (Zephyr thread entry points, which match Zephyr convention)
- Macro-defined Zephyr objects: `g_` prefix, all-lower snake case

The Zephyr-layer type aliases (`using LF = ...; using LCA = ...`) in
`app/zephyr_nucleo_h753zi/main.cpp:59–65` are pragmatic for a single-file
firmware but would be illegible if promoted to a multi-file translation unit.

### Header hygiene

Include guards use `#pragma once` consistently across new headers (e.g.,
`include/body_control/lighting/transport/doip_server.hpp:1`). The project does
not mix `#pragma once` with `#ifndef` guards. Platform headers (LwIP, Zephyr,
HAL) are included only in platform-specific source files, never in shared
headers.

### Error handling

The "return status codes, not exceptions" policy is applied consistently in the
core library. UDS handlers return `std::vector<uint8_t>` with the appropriate
NRC rather than throwing. The OTA session manager returns negative responses on
every error path. The DoIP server closes the connection on protocol violations
rather than throwing.

The one unresolved question is the `SomeipMessageParser` throw behaviour noted
in `app/zephyr_nucleo_h753zi/prj.conf`. If the parser is called from a path
marked `noexcept`, a thrown exception will call `std::terminate()` on Zephyr
rather than propagating. This needs explicit verification.

---

## 4  Test Coverage

### Inventory

The project has the following GoogleTest binaries (15 in total, not 14 as stated
in `README.md:41`):

| Binary | Test file |
|---|---|
| `test_lighting_payload_codec` | `test/unit/test_lighting_payload_codec.cpp` |
| `test_command_arbitrator` | `test/unit/test_command_arbitrator.cpp` |
| `test_fault_manager` | `test/unit/test_fault_manager.cpp` |
| `test_uds_request_handler` | `test/unit/test_uds_request_handler.cpp` |
| `test_ota_handler` | `test/unit/test_ota_handler.cpp` |
| `test_some_ip_sd_codec` | `test/unit/test_some_ip_sd_codec.cpp` |
| `test_someip_message_builder` | (unit) |
| `test_someip_message_parser` | (unit) |
| `test_node_health_state_manager` | (unit) |
| `test_lamp_status_state_manager` | (unit) |
| `test_blink_manager` | (unit) |
| `test_vss_lamp_overlay` | `test/test_vss_lamp_overlay.cpp` |
| `test_request_response_path` | `test/integration/test_request_response_path.cpp` |
| `test_controller_arbitration_via_operator` | `test/integration/test_controller_arbitration_via_operator.cpp` |
| `test_doip_server` | (unit) |

The README count of 14 predates the VSS test binary addition.

### Assertion quality

**Payload codec (`test_lighting_payload_codec.cpp`).** 7 tests. Round-trips for
`LampCommand`, `LampStatus`, `NodeHealthStatus`. Invalid boolean byte (0xAA
→ `kInvalidPayloadValue`). Wrong-length rejection. Null pointer rejection.
Invalid enum byte (0xFF). These are tight, specification-derived assertions —
exactly what the codec deserves.

**Command arbitrator (`test_command_arbitrator.cpp`).** 10 tests covering all
4 priority rules including hazard-blocks-indicator, indicator-exclusivity, and
the toggle-resolution path. The test `modified decision preserves source/seq`
verifies that arbitration does not corrupt metadata — a non-obvious correctness
property.

**Fault manager (`test_fault_manager.cpp`).** 13 tests. Idempotent inject,
independent multi-fault tracking, inject/clear/inject sequence, `GetFaultStatus`
content, `PopulateHealth` transitions. This is thorough coverage of the state
machine.

**UDS request handler (`test_uds_request_handler.cpp`).** 12 tests. Covers all
8 SIDs implemented. Gap: no test for `0x22` RDBI with DID `0xF100` (ECU
identification) or `0xF102` (node health with OTA mode active). The `EncodeNodeHealth`
OTA-mode branch (`src/application/uds_request_handler.cpp:344–347`) is
untested.

**OTA session manager (`test_ota_handler.cpp`).** 12 tests. Full transfer with
and without CRC, size mismatch, out-of-sequence block, post-complete state,
block sequence wrap at 0xFF → 0x00. The optional-CRC behaviour is tested: the
test `full transfer without CRC` confirms that a 1-byte 0x37 succeeds. This
makes the design decision explicit in the test suite — useful for reviewers but
also confirms the security gap identified in Section 1.

**SOME/IP-SD codec (`test_some_ip_sd_codec.cpp`).** 12 tests. Good coverage of
the encode path and the `DecodeOffer` round-trip. Missing: truncated options
section (entries_len consistent but options_len field truncated), zero
entries_len, entries_len not a multiple of 16, malformed option type field.

**VSS overlay (`test_vss_lamp_overlay.cpp`).** 7 tests. Per-lamp isolation,
all-faults snapshot, JSON shape verification. The cross-contamination checks
(test 1–5 each verify that exactly one lamp is on while others are off) are a
correct approach to testing the `IsLampOn` linear scan.

**Integration tests.** Two integration binaries use `LoopbackTransportAdapter`
for synchronous full-stack exercising. The loopback approach avoids test
non-determinism from real socket I/O. The integration test for
`test_controller_arbitration_via_operator` verifies that hazard blocks indicator
activation through the complete service chain — this is a portfolio-grade
integration test that demonstrates understanding of the operator service path.

### What is not tested

- DoIP server connection lifecycle (TCP accept, routing activation, framing
  errors, connection close)
- Zephyr-specific code paths (thread interactions, `k_msgq` overflow, mutex
  contention)
- SOME/IP-SD session ID increment across multiple encodes
- OTA Zephyr flash-write path (requires hardware or a Zephyr simulation target)
- `EncodeNodeHealth` with OTA mode active (health_state = 0x04)

---

## 5  CI / Static Analysis

### CI matrix

The GitHub Actions workflow
(`/home/pavankumar/workspace/Hobby-Projects/.github/workflows/build.yml`)
runs four jobs:

| Job | Trigger | Platform | Purpose |
|---|---|---|---|
| `linux-build` | push/PR to main | ubuntu-24.04 | cmake Release + ctest |
| `static-analysis-cppcheck` | push/PR to main | ubuntu-24.04 | cppcheck style/warning/perf/portability |
| `static-analysis-clang-tidy` | push/PR to main | ubuntu-24.04 | clang-tidy on selected src/ |
| `vss-build-test` | push/PR to main, needs linux-build | ubuntu-24.04 | cmake VSS=ON + ctest + --vss-snapshot smoke test |

**Zephyr is not in CI.** There is no west/sysbuild job building
`app/zephyr_nucleo_h753zi`. The GPIO pin comment discrepancy (overlay line 14
vs. line 24) and the fault propagation bug in `HealthThread` (see Section 1)
could both be caught by a Zephyr build + emulator test (QEMU ARM Cortex-M
simulation can run basic Zephyr networking tests without hardware).

### cppcheck configuration

The cppcheck job runs with `--enable=warning,style,performance,portability
--error-exitcode=1 --std=c++17`. The suppressions file
(`cppcheck-suppressions.txt`) curates project-level suppressions separately from
inline `// cppcheck-suppress` annotations. The inline suppression of
`useStlAlgorithm` in `src/vss/vss_lamp_overlay.cpp:18` and `:43` is
appropriate — the linear-scan `for` loops are intentional for embedded readability
and the suppression is documented.

### clang-tidy configuration

The `.clang-tidy` check set covers `bugprone-*`, `cert-*`,
`cppcoreguidelines` (3 specific checks), `misc-*`, `performance-*`,
`portability-*`. `WarningsAsErrors` is empty (not `'*'`), which means
clang-tidy warnings are not build-breaking. Promoting `WarningsAsErrors: '*'`
would harden the CI gate but would require addressing the current suppression
list first.

The clang-tidy job excludes platform files (STM32 HAL, Zephyr kernel, LwIP,
vsomeip) via the `find` path list in `build.yml:140–146`. This is correct —
those headers do not build on Linux and would produce false positives.

### Sanitizer and coverage gaps

Neither AddressSanitizer / UBSanitizer nor coverage measurement (lcov/gcov) is
configured in any CI job. For a portfolio project, ASan on the unit tests would
find any remaining buffer overread issues in codec parsing. Adding
`-fsanitize=address,undefined` as a Debug build CMake option with a separate CI
job is a one-afternoon addition that would materially strengthen the
"production-quality discipline" claim.

### vsomeip cache key

The vsomeip cache key (`vsomeip-3.4.10-ubuntu24`) is shared between the
`linux-build` and `static-analysis-clang-tidy` jobs but uses different `path`
lists. The `vss-build-test` job uses a more complete path list (includes
`/usr/local/include/vsomeip` and cmake/pkgconfig paths). If the cached install
from `linux-build` is missing those paths, `vss-build-test` will re-build
vsomeip unnecessarily. Unify the `path` entry across all four jobs that use it.

---

## 6  Security Posture

### Claimed controls vs. actual code

**ECDSA-P256 signing.** The MCUboot configuration
(`app/zephyr_nucleo_h753zi/sysbuild/mcuboot.conf:CONFIG_BOOT_SIGNATURE_TYPE_
ECDSA_P256=y`) enables ECDSA-P256 signature verification at boot. The signing
key is the MCUboot test key distributed with the Zephyr SDK — not a
project-private key. `doc/security_architecture.md` gap 5.4 acknowledges this
explicitly: "The current signing key is the MCUboot test key." This is the
correct posture for a portfolio project: the mechanism is in place, the
production gap is named.

**OTA integrity (block sequence + optional CRC).** Block sequence checking is
mandatory and correctly implemented in
`src/application/ota_session_manager.cpp:126–130` (wrong block sequence counter
→ NRC `0x73`). CRC-32/ISO-HDLC is software-computed, correct (Koopman
polynomial 0xEDB88320 at line 257), and applied to the running transfer. The
CRC validation at exit (`src/application/ota_session_manager.cpp:184`) is
conditional on `req.size() == 5U`. The server therefore accepts a compliant
transfer without CRC from any tester, which contradicts a "mandatory integrity
check" claim. For a test-lab tool this is acceptable; in a production OTA
server, CRC or HMAC validation should be unconditional.

**Anti-rollback.** MCUboot's `BOOT_UPGRADE_TEST` mode (called at
`app/zephyr_nucleo_h753zi/main.cpp` via `boot_request_upgrade(
BOOT_UPGRADE_TEST)`) requires the new image to confirm itself via
`boot_write_img_confirmed()` (called at line 789 after first successful health
TX). An unconfirmed image reverts on next boot. This is a working anti-rollback
mechanism. The `doc/security_architecture.md` note that there is no
monotonic anti-rollback counter is accurate — MCUboot with a test key and no
counter prevents accidental rollback but not deliberate downgrade attacks.

**UDS Security Access (0x27).** Not implemented. Correctly documented as gap
5.6. Any tester with network access can inject faults via 0x31 or initiate OTA
via 0x34 without authentication. On the hardware setup described (direct
Ethernet cable between host and NUCLEO), the attack surface is physically
bounded, but this is a production gap for any vehicle deployment.

**Transport-layer confidentiality.** All traffic is cleartext UDP/TCP. Gap 5.5
in `doc/security_architecture.md` acknowledges this. For a zonal architecture
where the node is inside the vehicle Ethernet backbone, cleartext is common
practice today, but ISO/SAE 21434 WP.4 (Identify cybersecurity goals) would
require a threat model entry for in-vehicle eavesdropping on diagnostic traffic.

**UN R155 / R156 mapping.** The security architecture document references R155
and R156 framework sections. The mapping is directionally correct: R156
(software updates) covers the OTA path; R155 (cybersecurity management) covers
the threat model. The gaps documented align with what a CSMS audit would find
(no cryptographic key management, no secure boot chain beyond MCUboot, no ECU
certificate provisioning).

**ISO 13400-2 (DoIP) security.** The DoIP server does not implement the
optional security TLS extension (ISO 13400-2:2019 Annex F). The routing
activation response is unconditional — any TCP client that sends a well-formed
routing activation request gains UDS access. This is standard for OBD tooling
on a vehicle's internal Ethernet but would require 0x27 gating in any
external-facing deployment.

---

## 7  Safety Posture

### ASIL classification and HARA

`safety/02_hazard_analysis_risk_assessment.md` defines six hazardous events:

| HE | Hazard | ASIL |
|---|---|---|
| HE-001 | Hazard lamps fail to activate | B |
| HE-002 | Indicator activates spuriously | A |
| HE-003 | Indicator stays active after deactivation | A |
| HE-004 | Headlamp deactivates unexpectedly | B |
| HE-005 | False hazard activation | A |
| HE-006 | All lamps fail simultaneously | A |

The ASIL B ceiling is defensible for exterior lighting. ASIL C/D would apply to
braking or steering. The ASIL decomposition across software component and GPIO
driver is not shown — the HARA establishes ASIL B at the system level but does
not carry it through to component-level ASIL allocation, which a real ISO 26262
Part 6 (software) analysis requires.

### FMEA and DTC traceability

The `safety/05_fmea.md` DTC values (0xB001–0xB005, one per lamp function) match
`include/body_control/lighting/domain/fault_types.hpp` enum values exactly.
They match the UDS RoutineControl routine IDs in
`src/application/uds_request_handler.cpp:38–43`. They match the Python
diagnostic client's fault injection commands. This three-way traceability
(FMEA → code → tooling) is the strongest safety artifact in the project.

**Stale reference.** `safety/05_fmea.md` FM-007 references
`src/application/ota_handler.cpp`. The actual file is
`src/application/ota_session_manager.cpp`. This stale reference would fail a
safety case document review.

### Open FMEA items vs. code

FM-002 (GPIO stuck-high, RPN 120) — no GPIO output readback in code. GPIO is
write-only. No detection mechanism exists in the current implementation.

FM-010 (absent IWDG, RPN 112) — `CONFIG_WATCHDOG=n` is not set in
`app/zephyr_nucleo_h753zi/prj.conf` (watchdog not explicitly enabled). The
Zephyr IWDG driver for STM32H7 requires `CONFIG_WATCHDOG=y` and a feed call in
the health thread or a dedicated watchdog thread. Not implemented, as documented.

FM-005 (network flood, RPN 96) — no rate limiting on UDP command ingress. The
`g_lamp_cmd_queue` has depth 8 (`app/zephyr_nucleo_h753zi/main.cpp:95`); excess
messages are dropped by `k_msgq_put` with `K_NO_WAIT` (or the equivalent
non-blocking API). This is a passive rate-limit — it prevents thread stack
overflow but does not prevent queue saturation under sustained flood.

### What a real ISO 26262 safety case needs

A genuine ISO 26262 Part 6 safety case for this component would additionally
require:

1. Software ASIL allocation per component (not just system-level ASIL B)
2. Evidence of software unit testing at the ASIL-appropriate coverage level
   (MC/DC for ASIL B)
3. Hardware-software interface (HSI) specification covering GPIO output states
   and failure modes
4. A software architectural design document tracing safety requirements to
   software components
5. Qualification of the compiler toolchain (or a tool confidence level argument)

None of these are present, which is correct for a portfolio project and is
acknowledged in `safety/README.md`.

---

## 8  SDV / VSS Integration Review

### vspec authoring

`vss/spec/bcl_lighting.vspec` defines standard VSS v6.0 signals:
`Vehicle.Body.Lights.Beam.Low.IsOn`, four indicator/hazard/park signals.
`vss/spec/bcl_extension.vspec` adds vendor-extension signals under
`Vehicle.Private.BCL.Lighting.*` — the correct namespace for non-standard OEM
signals per COVESA VSS conventions.

The `Vehicle.Body.Lights.Beam.Low.IsOn` mapping to `kHeadLamp`
(`src/vss/vss_lamp_overlay.cpp:57–58`) is a deliberate design decision
(documented in `vss/spec/bcl_lighting.vspec` comments). A VSS purist might
argue that a body-control headlamp is `Vehicle.Body.Lights.Beam.High.IsOn` or
a dedicated node, but the decision is defensible and explicitly documented.

The `IsDefect` signals defined in the vspec are not published at runtime. This
is documented as a limitation in `doc/vss_integration_design.md`. The fault
state is exposed via `Vehicle.Private.BCL.Lighting.*ActiveFaultCode` instead.

### CMake gating

The `BODY_CONTROL_LIGHTING_BUILD_VSS` option in the root `CMakeLists.txt` is a
tri-state: `AUTO` (detect Python + vss-tools), `ON` (require), `OFF` (skip).
This is the correct approach for a build dependency that requires a Python
toolchain — it allows the project to build on CI environments without
vss-tools without failing the configure step.

The `vss-build-test` CI job (`build.yml:148`) tests the `VSS=ON` path and
verifies `--vss-snapshot` is present in the `diagnostic_console` help output.
This is a minimal smoke test but it confirms the code-generation pipeline
produces the expected output.

### Code generation

`vss/generate_header.py` walks the JSON export from `vspec export json` and
emits `constexpr std::string_view` constants in namespace
`body_control::lighting::vss::paths`. The naming convention concatenates all
path components after removing dots — e.g.,
`kVehicleBodyLightsBeamLowIsOn`. This is readable for the VSS standard paths
and deliberately verbose for the private extension paths.

The generated constants are included only in `src/vss/vss_lamp_overlay.cpp`
(not in the header), which correctly confines the build-output dependency to
that single translation unit. Any rebuild of `bcl_vss_paths.hpp` triggers
recompilation of exactly one `.cpp` file.

### VssLampOverlay design

`VssLampOverlay` is a pure-computation adapter with no state, no I/O, and no
threads. `Snapshot()` at `src/vss/vss_lamp_overlay.cpp:52` takes
`std::array<LampStatus, 5>` and `LampFaultStatus` by const reference and
returns a `VssSnapshot` value type. `ToJson()` uses `std::ostringstream` —
allocating, Linux-only, and appropriate for a diagnostic console output path.
The design is correct: the overlay does not cache, does not own state, and
would be trivial to replace with a different serialization backend.

### Design doc vs. implementation discrepancy

`doc/vss_integration_design.md` Section 1 describes the overlay extension as
`Vehicle.Body.Lights.BCL.*`. The actual implementation uses
`Vehicle.Private.BCL.Lighting.*`. Section 3d of the design doc shows a
`VssLightSignals` struct with field names that differ from the actual
`VssSnapshot` struct. These discrepancies should be corrected — a reader
following the design doc will build an incorrect mental model of what the code
actually does.

---

## 9  Protocol / Transport Review

### SOME/IP service-data format

The project uses a 20-byte custom header for service data (not the standard
16-byte SOME/IP header used for SD traffic). This is a pragmatic choice for a
portfolio project and is documented in the codec header. The header layout
includes `service_id(2) + instance_id(2) + method_id(2) + client_id(2) +
session_id(2) + proto_version(1) + iface_version(1) + msg_type(1) +
return_code(1) + payload_length(4)` = 20 bytes. The 4-byte length field
supports payloads up to 4 GB, which is overkill for an embedded node but
matches the SOME/IP specification's field width.

The wire captures in `doc/captures/` verify that this framing is what actually
travels on the wire. The Wireshark Lua dissector in
`doc/captures/wireshark_dissector/` decodes it correctly.

### SOME/IP-SD compliance

`src/transport/some_ip_sd/some_ip_sd_codec.cpp` produces frames that conform to
the AUTOSAR AP_PRS_SOMEIPServiceDiscovery wire format. Specific points:

- Service ID 0xFFFF, Method ID 0x8100 in the SOME/IP header (line 30–31).
  These are the mandated values for SD traffic.
- Flags byte 0xC0 (reboot flag | unicast flag) at line 35 — correct for initial
  offer after device startup.
- `kOptContentLen = 0x0009` at line 45 — this is the non-obvious value that
  Wireshark validates. The comment explaining its derivation is accurate.
- Session ID is passed in from the caller and should be incremented per
  broadcast. Verify that the SOME/IP-SD broadcaster in the service layer
  increments session_id on each `EncodeOffer` call.

The `DecodeOffer` parser (`some_ip_sd_codec.cpp:208`) validates frame length,
magic bytes, entries_len, and entry type. One gap: `entries_len` is not
validated as a multiple of `kEntrySize` (16). A frame with `entries_len = 17`
would cause the loop at line 242 to read 1 complete entry and silently ignore
the trailing byte. Add `if ((entries_len % kEntrySize) != 0U) { return false;
}` before the loop.

### DoIP ISO 13400-2 compliance

`src/transport/doip_server.cpp` implements the subset of ISO 13400-2 required
for UDS pass-through:

- Protocol version 0xFD, inverse version 0x02 (line 275–276 in
  `BuildFrame`). Correct per Table 2 of ISO 13400-2:2019.
- Routing activation request (0x0005): validates length >= 7 bytes (line 191),
  reads tester logical address from bytes [0:1], responds with entity address
  `kDoipRearNodeAddress` and response code `kRoutingActivated` (0x10). The
  routing activation type field (byte [2], activation type) is not validated —
  the server accepts all activation types. Compliant behavior would return NRC
  for unsupported activation types (ISO 13400-2 Table 24 activation response
  code 0x06).
- Diagnostic message (0x8001): sends ACK before dispatching UDS
  (`src/transport/doip_server.cpp:241`). This is correct per ISO 13400-2 §7.6.
- The maximum payload cap at `kMaxPayloadLength = 65536U` in the Linux server
  is appropriate for DoIP. The Zephyr implementation uses a tighter
  `kDoipMaxPayloadLen = 1024U`
  (`app/zephyr_nucleo_h753zi/main.cpp:826`) which limits UDS request size but
  fits available Zephyr heap.

Not implemented (not claimed): vehicle announcement (0x0001), entity status
request (0x4001), diagnostic power mode (0x4003). These are optional for a
rear-node-only implementation and are appropriately omitted.

### UDS ISO 14229-1 NRC handling

`src/application/uds_request_handler.cpp` returns the correct NRC codes:

- `kNrcServiceNotSupported` (0x11) for unknown SIDs
- `kNrcSubFunctionNotSupported` (0x12) for unsupported session or routine
  sub-functions
- `kNrcRequestOutOfRange` (0x31) for malformed requests or unsupported DIDs
- `kNrcConditionsNotCorrect` (0x22) for state-machine violations (OTA outside
  session)
- `kNrcWrongBlockSequenceCounter` (0x73) for out-of-sequence 0x36 blocks
- `kNrcGeneralProgrammingFailure` (0x72) for flash write errors and CRC
  mismatch
- `kNrcUploadDownloadNotAccepted` (0x70) for 0x34 while OTA already active

The P2/P2* timing parameters in the 0x10 positive response
(`uds_request_handler.cpp:123–124`) — P2 = 25 ms (0x0019), P2* = 500 ms
(0x01F4) — are appropriate for a real-time embedded node. The Python client at
`tools/ota_client/ota_client.py` does not use these timing parameters
explicitly (it uses `socket.settimeout(10.0)`) but they are present in the
protocol response for any standards-compliant tester.

One gap: `HandleClearDiagnosticInformation` (0x14) only accepts group
`0xFFFFFF` (clear all). Per ISO 14229-1, a group-specific clear (e.g., clear
only powertrain DTCs) is optional but commonly expected. The current
implementation returns `kNrcRequestOutOfRange` for any group other than all.
This is valid behavior but limits compatibility with generic scan tools.

---

## 10  OTA / Firmware Update Review

### MCUboot integration (Zephyr)

The MCUboot configuration (`app/zephyr_nucleo_h753zi/sysbuild/mcuboot.conf`)
uses:
- `CONFIG_BOOT_SWAP_USING_MOVE=y` — no scratch partition required for the swap
  algorithm; moves sector-by-sector.
- `CONFIG_BOOT_MAX_IMG_SECTORS=8` — matches the STM32H753ZI bank layout (8
  sectors of 128 KB per bank).
- `CONFIG_BOOT_SIGNATURE_TYPE_ECDSA_P256=y`

The flash partition layout in the overlay file
(`app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay:100–115`) places slot0
at offset 0x40000 (768 KB), slot1 at 0x100000 (768 KB). Both slots are large
enough for the current image (~113 KB per the overlay comment) with room for
significant growth.

The `BOOT_UPGRADE_TEST` + `boot_write_img_confirmed()` pattern in
`app/zephyr_nucleo_h753zi/main.cpp:787–793` is the correct use of the MCUboot
test-then-confirm protocol. Confirmation is gated on a successful health event
transmission — this means the network stack, GPIO driver, and event dispatch
loop must all be healthy before the image is permanently accepted. This is a
genuine liveness proof, not a trivial `confirmed = true` at startup.

### UDS 0x34/0x36/0x37 sequence

The Zephyr OTA manager (`src/application/ota_session_manager_zephyr.cpp`) writes
incoming blocks to slot1 using `stream_flash_buffered_write()` with
`flush=false` during data blocks and `flush=true` on the final flush. The
`alignas(4) static std::uint8_t s_stream_buf[1024U]` buffer satisfies the
32-byte write alignment requirement for STM32H7 internal flash. After a
successful 0x37, `boot_request_upgrade(BOOT_UPGRADE_TEST)` marks slot1 as
pending, and a 100 ms delayed reboot is scheduled via `k_work_schedule`.

The Linux OTA manager (`src/application/ota_session_manager.cpp`) uses POSIX
`open()/write()/fsync()/rename()` to a staging file (`/tmp/
bcl_ota_staging.bin`). The `fsync` before `rename` is the correct
write-then-atomically-promote pattern for filesystem-backed staging.

**Block sequence wrap.** The sequence counter wraps 0xFF → 0x00 at
`ota_session_manager.cpp:151–152`. The test `block sequence wrap` in
`test_ota_handler.cpp` verifies this. The Python client at
`tools/ota_client/ota_client.py` uses `(block_seq & 0xFF)` for the same wrap.
The three are consistent.

**ECDSA validation order.** In the MCUboot `BOOT_UPGRADE_TEST` flow, signature
verification happens at boot time (in the MCUboot bootloader), not during the
UDS transfer. The UDS transfer writes raw flash bytes without verifying the
image signature. This is the standard MCUboot usage: the bootloader validates
before executing. However, it means a corrupted (but correctly sequenced and
CRC-matching) image will pass the UDS transfer, fail the MCUboot signature
check at next boot, and roll back. The `doc/security_architecture.md` gap 5.4
acknowledges the test key issue but does not explicitly note that signature
verification is deferred to boot time. Adding this clarification would prevent
misunderstanding.

### Python OTA client

`tools/ota_client/ota_client.py` uses `zlib.crc32()` which implements the same
CRC-32/ISO-HDLC polynomial as the C++ server code (`0xEDB88320`). The client
always sends the CRC in 0x37 (5-byte request), while the server accepts a
1-byte 0x37 without CRC. The `--dry-run` flag allows testing the DoIP/UDS
framing without writing to flash. The `DoIPClient` class is a clean context
manager with correct socket teardown.

---

## 11  Documentation Review

### README coherence

The `README.md` Skills Demonstrated table is accurate and specific — each row
links to a code artifact, not a generic claim. The "Known Issues" section is
unusually candid for a portfolio project; documenting the vsomeip IO thread
stall with root-cause tracing (LOG-0 through LOG-5) and clearly stating "why
it's not a project bug" is exactly the right framing.

The Hardware Platforms table and Wire Protocols section match the actual
implementation. The 12-phase roadmap is complete through Phase 12 (OTA) with
accurate status markers.

**Count error.** README line 41 says "14 GoogleTest unit tests." The actual
count is 15 test binaries (the VSS overlay test was added after the line was
written). Update to 15.

**Phase 13 reference.** Several files and comments reference "Phase 13" (DoIP/
UDS on Zephyr, OTA), but the roadmap table ends at Phase 12. Add Phase 13 to
the roadmap.

### Code-documentation discrepancies

`doc/vss_integration_design.md` describes the overlay extension namespace as
`Vehicle.Body.Lights.BCL.*` (Section 1) but the actual namespace is
`Vehicle.Private.BCL.Lighting.*` (confirmed in `vss/spec/bcl_extension.vspec`
and `src/vss/vss_lamp_overlay.cpp`). The struct name `VssLightSignals` in the
design doc (Section 3d) does not exist in the code; the implementation uses
`VssSnapshot`. These two discrepancies are the most significant documentation
accuracy issues in the project.

`safety/05_fmea.md` FM-007 cites `src/application/ota_handler.cpp` which does
not exist. The file is `src/application/ota_session_manager.cpp`.

The overlay comment at
`app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay:14` says `PB5 → Left
Indicator`. The DTS node at line 24 assigns `left_indicator` to `&gpioa 5` not
`&gpiob 5`. One of them is wrong; verify against the physical wiring.

### Skills table accuracy

The table entry for "Testing" (README line 41) says 14 tests (correct to 15 as
noted). The entry for "Static analysis" is accurate. The entry for "MCUboot
OTA" is accurate. The entry for "Zephyr RTOS" correctly describes the
threading model. The entry for "SDV signal modeling" correctly describes the
VSS integration.

The entry for "Adaptive AUTOSAR" correctly says "proxy/skeleton pattern" without
claiming generated stubs. An interviewer will probe whether this means
hand-written or toolchain-generated — the answer (hand-written, ARXML is
documentation only) is honest and should be rehearsed.

---

## 12  Portfolio Positioning

### Five strongest talking points with file evidence

**1. Genuine three-target portability from a single codebase.**
The same `ExteriorLightingFunctionManager`, `CommandArbitrator`,
`FaultManager`, and `UdsRequestHandler` compile unmodified for Linux (vsomeip),
STM32 bare-metal (LwIP), and Zephyr RTOS (BSD sockets). The
`TransportAdapterInterface`
(`include/body_control/lighting/transport/transport_adapter_interface.hpp`) is
the mechanism. In an interview: point to this file and walk through the
dependency inversion — service layer depends on an interface, platform layer
provides the concrete implementation, no `#ifdef` in the logic.

**2. Wire-level protocol implementation with Wireshark evidence.**
SOME/IP-SD frames are documented in `src/transport/some_ip_sd/some_ip_sd_codec.
cpp` with the non-obvious `kOptContentLen = 0x0009` derivation. DoIP framing
conforms to ISO 13400-2 protocol version 0xFD. The `doc/captures/` directory
contains real Wireshark captures from running hardware. This is not simulation —
it is evidence that the protocols work on real Ethernet.

**3. End-to-end OTA with MCUboot test-then-confirm.**
The `BOOT_UPGRADE_TEST` + `boot_write_img_confirmed()` pattern in
`app/zephyr_nucleo_h753zi/main.cpp:787–793` demonstrates understanding of the
MCUboot liveness proof protocol. The Python OTA client demonstrates the full
0x34/0x36/0x37 UDS sequence. The flash partition layout in the overlay matches
the STM32H753ZI dual-bank topology. This is a complete OTA story that most
portfolio projects do not have at all.

**4. FMEA-to-code traceability.**
DTC values 0xB001–0xB005 in
`include/body_control/lighting/domain/fault_types.hpp`, routine IDs 0xB001–
0xB005 in `src/application/uds_request_handler.cpp:38–43`, and FM-001–FM-005 in
`safety/05_fmea.md` form a traceable chain from hazard analysis through software
implementation to diagnostic tooling. This is the kind of cross-artifact
traceability that automotive safety engineers look for.

**5. Honest gap documentation.**
The security architecture document names 8 gaps with production fix paths. The
FMEA names 3 open RPN items. The Known Issues section in README explains a
vsomeip IO thread stall with root-cause evidence. Engineers who document gaps
explicitly demonstrate the discipline that production programs require.

### Five most likely senior-interviewer probes

**Probe 1:** "Walk me through how a LampCommand gets from the HMI button press
to the GPIO toggle on the NUCLEO." This tests end-to-end system understanding.
Expected answer: HMI → OperatorServiceConsumer → UDP → OperatorServiceProvider
(CZC) → CommandArbitrator → FunctionManager → SOME/IP event → Zephyr UDP RX
thread → g_lamp_cmd_queue → cmd_thread → BlinkManager → ZephyrGpioDriver.
Cite `app/zephyr_nucleo_h753zi/main.cpp:693–713` for the cmd_thread dequeue
loop.

**Probe 2:** "The ARXML descriptions — are they inputs to a code generator or
documentation only?" Expected answer: documentation only; the proxy/skeleton
facades are hand-written. An interviewer from an Adaptive AUTOSAR toolchain
background (Vector, ETAS) will respect the honest answer over an inflated claim.

**Probe 3:** "How does MCUboot know the new image is good before permanently
committing it?" Expected answer: `BOOT_UPGRADE_TEST` marks slot1 as
trial-boot; the application must call `boot_write_img_confirmed()` before the
next reboot or MCUboot reverts to slot0. Confirmation is gated on a successful
health event transmission in `HealthThread`.

**Probe 4:** "You have `health.lamp_driver_fault_present = false` hard-coded in
the health thread — what would you do to fix that?" This probes whether the
candidate has read their own code at depth. Expected answer: call
`g_lamp_mgr.GetFaultStatus()` inside the mutex in HealthThread, then call
`FaultManager::PopulateHealth()` on the result before building the
`NodeHealthStatus` message.

**Probe 5:** "Why does `EncodeNodeHealth` hard-code `eth_link` and `svc_avail`
as always-up?" (`uds_request_handler.cpp:354`: `flags = 0x03U | fault_bit`.)
Expected answer: the UDS handler does not have access to the transport layer's
link state. Fixing this requires injecting a link-status callback or querying
`ZephyrUdpTransportAdapter` for its `NET_IF_RUNNING` state. It is a
known gap between the UDS diagnostic view and actual runtime state.

---

## 13  Prioritized Recommendations

Priority codes: **P1** = must fix before showing to hiring panel; **P2** = fix
before technical interview; **P3** = nice-to-have; **P4** = defer.

| Pri | Title | Rationale | Effort | Files |
|---|---|---|---|---|
| P1 | Fix fault-state propagation in Zephyr HealthThread | HealthThread hard-codes fault_present=false. DTCs injected via UDS never appear in health events. Functional correctness bug. | 30 min | `app/zephyr_nucleo_h753zi/main.cpp:765–769` |
| P1 | Resolve GPIO pin comment vs. DTS discrepancy | Overlay comment says PB5 for left_indicator; DTS node assigns gpioa 5. One is wrong. Verify against physical wiring and correct both. | 15 min | `app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay:14,24` |
| P1 | Fix stale FMEA file reference | `safety/05_fmea.md` FM-007 cites `ota_handler.cpp` which does not exist. Correct to `ota_session_manager.cpp`. | 5 min | `safety/05_fmea.md` |
| P1 | Fix VSS design doc namespace and struct name | `doc/vss_integration_design.md` uses `Vehicle.Body.Lights.BCL.*` and `VssLightSignals`. Actual code uses `Vehicle.Private.BCL.Lighting.*` and `VssSnapshot`. | 20 min | `doc/vss_integration_design.md` |
| P1 | Update README test count and add Phase 13 to roadmap | README says 14 tests (actual: 15). Phase 13 is referenced in code comments but absent from the roadmap table. | 10 min | `README.md:41, README.md:roadmap table` |
| P2 | Make OTA CRC validation unconditional on server | `ota_session_manager.cpp:184` validates CRC only if `req.size() == 5U`. A 1-byte 0x37 bypasses integrity check. Change to require exactly 5 bytes or reject. | 30 min | `src/application/ota_session_manager.cpp:183–203` |
| P2 | Add `entries_len % kEntrySize == 0` guard in DecodeOffer | Non-multiple entries_len will cause a partial-entry read in the loop. Add the modulo check before the loop. | 15 min | `src/transport/some_ip_sd/some_ip_sd_codec.cpp:241` |
| P2 | Verify noexcept correctness on Zephyr paths that call SomeipMessageParser | `prj.conf` enables exceptions for the parser. Any upper-layer function marked `noexcept` that transitively calls the parser will `std::terminate()` on a throw. | 1 hour | `src/platform/zephyr/zephyr_udp_transport.cpp`, `app/zephyr_nucleo_h753zi/main.cpp` |
| P2 | Unify vsomeip cache `path` across all CI jobs | `linux-build` caches only `libvsomeip3*`. `vss-build-test` caches headers and cmake config too. Mismatched cache scope causes unnecessary re-builds. | 15 min | `.github/workflows/build.yml` |
| P3 | Add ASan/UBSan CI job | `cmake -DCMAKE_BUILD_TYPE=Debug -fsanitize=address,undefined` + ctest. Catches any remaining buffer overreads in codec parsers. | 2 hours | `.github/workflows/build.yml`, `CMakeLists.txt` |
| P3 | Add SOME/IP-SD malformed-frame fuzz tests | `DecodeOffer` lacks tests for zero entries_len, non-multiple entries_len, truncated options section, unknown option type. These are specification-boundary conditions. | 3 hours | `test/unit/test_some_ip_sd_codec.cpp` |
| P3 | Test EncodeNodeHealth with OTA mode active | `uds_request_handler.cpp:344` OTA mode sets health_state to 0x04 but this branch has no unit test coverage. | 30 min | `test/unit/test_uds_request_handler.cpp` |
| P3 | Replace `reinterpret_cast` in EncodeEcuIdentification | `uds_request_handler.cpp:389–391` casts `const char*` to `const uint8_t*`. Replace with `std::memcpy` into a pre-sized vector. Eliminates the only non-platform `reinterpret_cast`. | 15 min | `src/application/uds_request_handler.cpp:382–391` |
| P3 | Replace `static_cast<std::size_t>(-1)` sentinel with `std::optional<std::size_t>` | `FaultManager::LampFunctionToIndex` relies on SIZE_MAX as an out-of-band sentinel. `std::optional` makes the absence-case explicit in the type. | 45 min | `src/application/fault_manager.cpp` |
| P4 | Add Zephyr QEMU job to CI | Requires setting up a Zephyr SDK Docker container and running `west build -b qemu_cortex_m3` (or equivalent). Would catch build regressions on the Zephyr target early. | 4 hours | `.github/workflows/build.yml` |
| P4 | Decompose `app/zephyr_nucleo_h753zi/main.cpp` | 1130-line single-file firmware. Thread entry points, DoIP framing, SOME/IP-SD broadcasting could each be separate translation units. Not a correctness issue; a maintainability issue. | 4 hours | `app/zephyr_nucleo_h753zi/main.cpp` |
| P4 | Document that signature verification is deferred to MCUboot boot time | `doc/security_architecture.md` gap 5.4 discusses the test key but does not note that signature verification happens at boot, not during UDS transfer. A reader may believe the server validates the image during 0x36 blocks. | 15 min | `doc/security_architecture.md` |

**Must-fix before applying (P1 items summary).** Five items: fault propagation
bug, GPIO pin comment/DTS discrepancy, FMEA stale reference, VSS design doc
inaccuracies, README count error. None require structural changes. All can be
addressed in under two hours total.

**Nice-to-have (P3).** The ASan CI job and SOME/IP-SD fuzz tests would
materially strengthen the "production discipline" claim. The `std::optional`
refactor is low-risk and removes an implicit contract.

**Defer (P4).** Zephyr QEMU CI and main.cpp decomposition are real improvements
but require non-trivial toolchain setup and refactoring work that does not
change the project's demonstrable capabilities before a job application.

---

## 14  Appendix


The codebase is unusually clean for a solo portfolio project. A full grep for
`TODO`, `FIXME`, `XXX`, and `HACK` across `src/`, `include/`, `app/`, and
`test/` returns zero results. The only development artifacts found are:

- `src/application/ota_session_manager_stm32.cpp` — documented stub returning
  NRC 0x22 for all OTA services on bare-metal STM32. Not a TODO; it is an
  intentional placeholder with a documented rationale (no POSIX file I/O on
  bare-metal). The comment at line 1 is explicit about this.

- Inline `// cppcheck-suppress useStlAlgorithm` annotations in
  `src/vss/vss_lamp_overlay.cpp:18` and `:43`. These are deliberate
  suppressions for the linear-scan `for` loops, not deferred work.

- `// NOLINTNEXTLINE(cert-err34-c)` in
  `src/transport/some_ip_sd/some_ip_sd_codec.cpp:85` for the `sscanf` call.
  Correctly suppressed with a documented rationale (range check follows
  immediately).

### Dead code

No dead code found in reviewed files. The `kUnknown = 0` sentinels are used
in every switch/if-else chain that processes `LampFunction` or `FaultCode` —
they are not dead, they are the catch-all paths.

The `ota_session_manager_stm32.cpp` stub methods are technically "dead" in the
sense that they always return NRC without doing real work, but they are the
correct STM32 bare-metal implementation for a system that does not support OTA
via bare-metal (only via Zephyr MCUboot).

### Debug prints

`std::cout` and `std::cerr` are used in Linux-target source files
(`src/transport/doip_server.cpp:76`, `src/application/ota_session_manager.cpp:
80`, `src/application/ota_session_manager.cpp:217`). These are appropriate for
the Linux diagnostic path — the DoIP server prints port binding confirmation
and the OTA manager prints transfer completion. On the Zephyr target,
`LOG_INF`/`LOG_DBG`/`LOG_WRN` macros are used throughout
`app/zephyr_nucleo_h753zi/main.cpp`, which is correct (Zephyr's logging
subsystem is the right output path on embedded).

No gratuitous debug `printf` / `cout` patterns were found. The Zephyr target
uses `LOG_DBG` (filtered at INFO level by default) for verbose traces that
would be filtered in production.

### Magic numbers

The codebase uses named constants (`constexpr`) consistently in reviewed files.
The one instance worth noting: `app/zephyr_nucleo_h753zi/main.cpp:764`
(`NHState::kOperational`) combined with `health.active_fault_count = 0U` are
literal values that should be replaced by a `FaultManager::PopulateHealth()`
call (see P1 recommendation). They are magic in the sense that they bypass the
actual fault state.

The timing constants in `uds_request_handler.cpp:123–124` (`0x00U, 0x19U`
for P2 = 25 ms, `0x01U, 0xF4U` for P2* = 500 ms) are ISO 14229-1 timing
parameters embedded directly as bytes. They could be named constants. This is
a minor maintainability concern, not a correctness issue.

### Naming inconsistencies

- The Linux OTA session manager file is `ota_session_manager.cpp`; the Zephyr
  version is `ota_session_manager_zephyr.cpp`; the STM32 stub is
  `ota_session_manager_stm32.cpp`. The naming pattern is consistent and
  descriptive.

- `g_last_hazard_sequence` in `app/zephyr_nucleo_h753zi/main.cpp` — a file-
  scope variable used for companion command suppression that is not exposed in
  any header interface. The `g_` prefix is used for Zephyr file-scope
  globals throughout the file, so this is consistent with the local convention,
  but the variable carries a subtle behavioral contract (suppressing the
  companion set/clear when hazard is active) that is only discoverable by
  reading the code rather than the interface.

- The CMake option is `BODY_CONTROL_LIGHTING_BUILD_VSS`. The VSS CI job is
  `vss-build-test`. The feature branch references are `VSS=ON`. All three
  refer to the same capability under three slightly different names, but they
  are not ambiguous in context.

---

*End of review.*

# Body Control Zonal Lighting

[![Build](https://github.com/satyanar-lab/Hobby-Projects/actions/workflows/build.yml/badge.svg)](https://github.com/satyanar-lab/Hobby-Projects/actions/workflows/build.yml)

An automotive-style software demonstration built around a familiar body-control
problem — indicators, hazard, park lamps, headlamps — redesigned around the
ideas the industry is actually moving toward: **zonal architecture**,
**Ethernet/service-oriented communication**, and **software-defined vehicle**
practices. Linux-first for fast iteration, portable to an **STM32
NUCLEO-H753ZI** rear-node target.

> **Owner:** Pavankumar Satyanarayan · portfolio project

---

## Skills Demonstrated

This project demonstrates skills relevant to AUTOSAR
Integration Engineer (Ethernet/SDV) roles. Each row
links to specific evidence in the codebase.

| Skill area | Evidence in this project |
|---|---|
| **Adaptive AUTOSAR** | Service-oriented architecture with proxy/skeleton pattern. ARXML interface descriptions in [arxml/](arxml/) describe ExteriorLightingService and OperatorService following AUTOSAR R20-11 schema. |
| **SOME/IP** | Custom SOME/IP-shaped UDP transport between controller and rear node. Wire-format documented in [include/.../lighting_payload_codec.hpp](include/body_control/lighting/domain/lighting_payload_codec.hpp). Custom Wireshark Lua dissector at [doc/captures/wireshark_dissector/](doc/captures/wireshark_dissector/) decodes the framing. |
| **Automotive Ethernet** | Real Ethernet traffic between Linux host (192.168.0.10) and STM32 NUCLEO-H753ZI (192.168.0.20) at 100 Mbps full-duplex. Captures in [doc/captures/](doc/captures/) prove the wire-level behavior. |
| **DoIP (ISO 13400-2)** | TCP server on port 13400 with routing activation, diagnostic message dispatch. Implementation in [src/transport/](src/transport/) for Linux, [app/stm32_nucleo_h753zi/main.cpp](app/stm32_nucleo_h753zi/main.cpp) for STM32, [app/zephyr_nucleo_h753zi/main.cpp](app/zephyr_nucleo_h753zi/main.cpp) for Zephyr. |
| **UDS (ISO 14229-1)** | Services 0x10 (Diagnostic Session Control), 0x14 (Clear DTC), 0x19 (Read DTC), 0x22 (Read Data By Identifier), 0x31 (Routine Control), 0x34/0x36/0x37 (OTA download). Handler at [src/application/uds_request_handler.cpp](src/application/uds_request_handler.cpp). |
| **Embedded C++ (modern)** | C++17 throughout, RAII resource management, std::variant for type-safe message dispatch, no exceptions, no dynamic allocation in critical paths. Builds for Linux, STM32 bare-metal, and Zephyr RTOS from a shared codebase. |
| **MISRA-oriented discipline** | Explicit interfaces, deterministic behavior, low coupling, clean separation between domain/application/service/transport layers. Static analysis ready. |
| **Adaptive AUTOSAR communication patterns** | Request-response (UDS), publish-subscribe (LampStatusEvent, NodeHealthStatusEvent), service discovery hooks. Three communication patterns implemented. |
| **MCUboot OTA** | Full bootloader integration on Zephyr with ECDSA-P256 signing, dual-bank flash, swap-using-move algorithm, automatic rollback. End-to-end OTA verified on STM32 hardware. See [doc/mcuboot_integration.md](doc/mcuboot_integration.md). |
| **Diagnostic Trouble Codes** | FaultManager with DTCs B1001–B1005 (one per lamp). Fault injection via UDS 0x31 RoutineControl. Implementation at [src/application/fault_manager.cpp](src/application/fault_manager.cpp). |
| **Zephyr RTOS** | Multi-threaded application with msgq IPC, devicetree GPIO, NET_IF_RUNNING gating, MCUboot integration, BSD sockets. See [app/zephyr_nucleo_h753zi/](app/zephyr_nucleo_h753zi/). |
| **STM32 bare-metal** | LwIP raw API TCP server, HAL GPIO, custom linker script with MCUboot-compatible offsets, USART3 retarget. See [app/stm32_nucleo_h753zi/](app/stm32_nucleo_h753zi/). |
| **HMI / Qt6** | QML-based control panel with atomic state and 80ms poll timer. Drives all three backends interchangeably. See [app/hmi_control_panel/](app/hmi_control_panel/). |
| **Build systems** | CMake for Linux + STM32 bare-metal cross-compilation. Zephyr west sysbuild for MCUboot child image build. CI/CD with GitHub Actions. |
| **Testing** | 12 GoogleTest unit tests covering arbitrator, function manager, fault manager, UDS handler, OTA handler, payload codec. Runs in CI on every push. |
| **Tooling** | Python UDS client, Python OTA client, Wireshark dissector, sysbuild, west, STM32CubeProgrammer integration, Lauterbach-friendly elf output. |

### Hardware platforms

- **Linux x86_64** — full simulator with vsomeip 3.4.10
- **STM32 NUCLEO-H753ZI** bare-metal — LwIP raw API
- **STM32 NUCLEO-H753ZI** Zephyr RTOS — BSD sockets, MCUboot

### Wire protocols verified

- SOME/IP-shaped UDP on ports 41000/41001 (custom framing)
- DoIP over TCP port 13400 (ISO 13400-2)
- UDS over DoIP (ISO 14229-1)

Wireshark captures with screenshots in [doc/captures/](doc/captures/).

### Documentation

- [arxml/README.md](arxml/README.md) — AUTOSAR ARXML mapping to C++
- [doc/captures/README.md](doc/captures/README.md) — Wire-level evidence
- [doc/mcuboot_integration.md](doc/mcuboot_integration.md) — Bootloader integration
- [doc/ota_specification.md](doc/ota_specification.md) — UDS OTA flow

---

## Why this project exists

A small, familiar feature rebuilt as if it were part of a modern automotive
program, so the architectural story is concrete instead of abstract:

- Why the industry is moving from isolated CAN-oriented ECU nodes toward
  zonal compute with Ethernet backbones.
- How the same feature looks when it is expressed through service-oriented
  request/response + pub/sub (SOME/IP-shaped) rather than bare CAN frames.
- How a clean layering (domain → application → service → transport →
  platform) lets the same code compile for a Linux simulator and a real
  microcontroller target with no churn in the upper layers.

## What you run

Five executables, each a thin `main()` over the shared core library:

| Executable | Role |
|---|---|
| `central_zone_controller_app` | Decision-making controller. Arbitrates commands, talks to the rear node, caches state and health, fans events to all connected operator clients. |
| `exterior_lighting_node_simulator` | Rear-node service provider (Linux). Applies lamp commands, publishes status + health events. Replaced by the STM32 firmware on hardware. |
| `hmi_control_panel_qt` | Qt6 QML GUI operator panel. Sends user intents to the controller and reflects live lamp state with an automotive-style dark dashboard. Requires Qt6. |
| `hmi_control_panel_terminal` | Terminal-menu HMI fallback. Same operator service path as the Qt HMI; used when Qt6 is not available. |
| `diagnostic_console` | Engineering console. Lets you poke the service path and inspect node health directly. |

All five share one static library, `body_control_lighting::core`, so the
feature logic lives in one place and executables are just wiring.

## Layering

```
include/body_control/lighting/
├── domain/        # value types, constants, on-wire codec, service IDs
├── application/   # arbitrator, state manager, health monitor, controller, function manager
├── hmi/           # view model, mapper, main window
├── service/       # rear-lighting + operator service provider + consumer facades
├── transport/     # SOME/IP-style message builder/parser + adapter interface
└── platform/
    ├── linux/     # clock, logger, signal handler (POSIX)
    └── stm32/     # GPIO driver, link supervisor, UART-style logger (embedded)
src/
└── (mirrors the include tree, plus transport/ethernet, transport/vsomeip, transport/lwip)
```

See `doc/system_architecture.md` for the architectural rationale, and
`doc/service_interface_specification.md` for the service contract.

Real Wireshark packet captures from the running system (SOME/IP-shaped UDP
and UDS over DoIP) are in `doc/captures/` — see
`doc/captures/README.md` for details.

AUTOSAR R20-11 service interface descriptions (ARXML) are in `arxml/` —
see `arxml/README.md` for the mapping between ARXML elements and C++ classes.

## Build — Linux

```bash
mkdir build && cd build
cmake ..
cmake --build . -j
ctest --output-on-failure
```

CMake options:

| Option | Default | Purpose |
|---|---|---|
| `BODY_CONTROL_LIGHTING_BUILD_TESTS` | `ON` | Build GoogleTest-based unit + integration tests |
| `BODY_CONTROL_LIGHTING_BUILD_APPS` | `ON` | Build the five executables |
| `BODY_CONTROL_LIGHTING_BUILD_QT_HMI` | `ON` | Build the Qt6 QML HMI (requires Qt6 Core/Quick/Qml) |
| `BODY_CONTROL_LIGHTING_WARNINGS_AS_ERRORS` | `ON` | Promote `-Wall -Wextra -Wpedantic` warnings to errors |
| `BODY_CONTROL_LIGHTING_TARGET_PLATFORM` | auto (`linux` on a Linux host) | Selects platform-specific sources (`linux` or `stm32`) |

GoogleTest is fetched automatically via CMake `FetchContent` — no system
install needed.

## Running the Linux demo

**Start order matters.** The rear node must be up before the controller
connects; the controller must be up before the HMI or diagnostic console
send their first request.

In four terminals:

```bash
# Terminal 1 — rear node (must be first)
./build/app/exterior_lighting_node_simulator

# Terminal 2 — controller (must start before HMI / diagnostic console)
./build/app/central_zone_controller_app

# Terminal 3 — HMI operator panel (Qt6 GUI)
./build/app/hmi_control_panel_qt

# Terminal 3 — HMI operator panel (terminal fallback, same service path)
./build/app/hmi_control_panel_terminal

# Terminal 4 — engineering console (optional)
./build/app/diagnostic_console
```

The HMI and diagnostic console are thin operator clients: they send lamp
requests over the operator service path (UDP :41003 → :41002) and receive
`LampStatus` / `NodeHealth` events back.  The controller arbitrates every
request, talks to the rear node over the rear lighting service path
(UDP :41001 → :41000), and fans status events out to all connected operator
clients.  All four processes can be stopped with `Ctrl-C`.

## Engineering rules

This repository is built under a specific discipline and the code review
standard is "would this look in place in a real automotive program":

- **MISRA-oriented C++:** scoped enums with explicit underlying types,
  `kUnknown = 0` reserved so a zeroed message never looks valid, no raw
  owning pointers in interfaces, `noexcept` on leaf functions where it is
  honest, `[[nodiscard]]` on status-returning functions, default member
  initialisers on every POD field, no `using namespace` in headers.
- **Deterministic layering:** `domain` knows nothing; `application` depends
  only on `domain`; `service` depends on `application` + `transport` +
  `domain`; platform code is isolated behind interfaces and selected at
  configure time, not with `#ifdef` scattered through logic.
- **Explicit contracts:** every struct field, every enum value, every
  on-wire byte has a documented meaning. The codec header describes the
  payload layout; the parser/builder match it; the tests assert it.
- **Honest failure:** functions that can fail return status codes, not
  exceptions. The service provider's fallback health snapshot reports
  `kDegraded` when it can't see the world, not `kOperational`.
- **Engineering comments on every file:** every class, public function,
  non-obvious code block, and domain struct field is annotated — not with
  WHAT the code does, but WHY it is shaped the way it is.

## Roadmap

| Phase | Status | What it delivers |
|---|---|---|
| 1 — Foundation | ✅ Complete | Layered tree, CMake, core domain/service/transport scaffolding |
| 2 — Core logic + service path | ✅ Complete | Domain contracts locked, codec, arbitrator, state manager, health monitor, rear lighting service, unit + integration tests |
| 3 — Simulation integration | ✅ Complete | Synchronous loopback, round-trip event assertions, periodic publish loop, smoke test |
| 4 — Operator service layer | ✅ Complete | Dedicated HMI ↔ controller service path (ports 41002/41003); operator service consumer/provider |
| 5 — Real vsomeip transport | ✅ Complete | Real vsomeip 3.4.10; hazard expansion to 3 commands; indicator exclusivity fix; run scripts |
| 6 — STM32 hardware | ✅ Complete | NUCLEO-H753ZI bare-metal firmware; LwIP/UDP; GPIO lamp driver; blink manager; status events to CZC |
| 7 — Demo polish | ✅ Complete | End-to-end status events from NUCLEO to HMI; hardware walkthrough documentation |
| 8 — Qt6 GUI HMI | ✅ Complete | Qt6 QML dark dashboard; QmlHmiBridge; thread-safe callbacks; terminal HMI kept as fallback |
| 9 — Zephyr RTOS + HMI fixes | ✅ Complete | Zephyr RTOS port (4-thread, devicetree GPIO, message queue); turn-signal retention; HMI persistent poll timer; leftArrowActive/rightArrowActive display state; automotive button styling |
| 10 — Fault injection | ✅ Complete | FaultManager (DTC storage, inject/clear/clear-all); fault commands on both service paths; NodeHealthStatus fault fields + Qt HMI fault panel; diagnostic console fault menu; 17 new unit tests |
| 11 — UDS diagnostics | ✅ Complete | DoIP TCP server (ISO 13400-2) on rear node; UDS services 0x10/0x14/0x19/0x22/0x31; Python diagnostic client; 14 new unit tests |
| 12 — OTA firmware update | ✅ Complete | UDS 0x34/0x36/0x37 over DoIP; OtaSessionManager with CRC-32 validation; Python OTA client; kUpdating health state; 12 new unit tests |

## License

TBD — project is currently for personal portfolio use. Do not redistribute
without contacting the owner.

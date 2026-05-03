# Body Control Zonal Lighting

An automotive-style software demonstration built around a familiar body-control
problem — indicators, hazard, park lamps, headlamps — redesigned around the
ideas the industry is actually moving toward: **zonal architecture**,
**Ethernet/service-oriented communication**, and **software-defined vehicle**
practices. Linux-first for fast iteration, portable to an **STM32
NUCLEO-H753ZI** rear-node target.

> **Owner:** Pavankumar Satyanarayan · portfolio project

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
| `rear_lighting_node_simulator` | Rear-node service provider (Linux). Applies lamp commands, publishes status + health events. Replaced by the STM32 firmware on hardware. |
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
./build/app/rear_lighting_node_simulator

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

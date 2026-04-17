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

Four executables, each a thin `main()` over the shared core library:

| Executable | Role |
|---|---|
| `central_zone_controller_app` | Decision-making controller. Arbitrates commands, talks to the rear node, caches state and health. |
| `rear_lighting_node_simulator` | Rear-node service provider. Applies lamp commands, publishes status + health events. Swapped for the STM32 target in the hardware phase. |
| `hmi_control_panel` | Laptop-side operator UI. Sends user intents to the controller and reflects the latest state. |
| `diagnostic_console` | Engineering console. Lets you poke the service path and inspect node health directly. |

All four share one static library, `body_control_lighting::core`, so the
feature logic lives in one place and executables are just wiring.

## Layering

```
include/body_control/lighting/
├── domain/        # value types, constants, on-wire codec, service IDs
├── application/   # arbitrator, state manager, health monitor, controller, function manager
├── hmi/           # view model, mapper, main window
├── service/       # rear-lighting service provider + consumer (facades)
├── transport/     # SOME/IP-style message builder/parser + adapter interface
└── platform/
    ├── linux/     # clock, logger, signal handler (POSIX)
    └── stm32/     # GPIO driver, link supervisor, UART-style logger (embedded)
src/
└── (mirrors the include tree, plus transport/ethernet and transport/vsomeip)
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
| `BODY_CONTROL_LIGHTING_BUILD_APPS` | `ON` | Build the four executables |
| `BODY_CONTROL_LIGHTING_WARNINGS_AS_ERRORS` | `OFF` | Promote `-Wall -Wextra -Wpedantic` warnings to errors |
| `BODY_CONTROL_LIGHTING_TARGET_PLATFORM` | auto (`linux` on a Linux host) | Selects platform-specific sources (`linux` or `stm32`) |

GoogleTest is fetched automatically via CMake `FetchContent` — no system
install needed.

## Running the Linux demo

In four terminals (or with the helper scripts under `tools/`):

```bash
./build/app/rear_lighting_node_simulator     # start the node first
./build/app/central_zone_controller_app      # controller connects
./build/app/hmi_control_panel                # operate the lamps
./build/app/diagnostic_console               # optional: introspect
```

The HMI drives the feature; the controller arbitrates and forwards commands
over the service path; the simulator applies them and publishes status/health
back. All four can be stopped with `Ctrl-C` / `ENTER`.

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

## Roadmap

| Phase | Status | What it delivers |
|---|---|---|
| 1 — Foundation | ✅ Complete | Layered tree, CMake, core domain/service/transport scaffolding |
| 2 — Core logic + service path | ✅ In finalisation | Domain contracts locked, SOME/IP-style wire format aligned, tests green, CTest wired |
| 3 — Simulation integration | 🔄 In progress | End-to-end command + status + health flow across the four executables |
| 4 — Ethernet / vsomeip upgrade | ⏳ Planned | Replace loopback/UDP with real vsomeip behaviour and SD |
| 5 — Hardware migration | ⏳ Planned | STM32 NUCLEO-H753ZI as the rear node (LwIP + GPIO driver) |
| 6 — Demo + polish | ⏳ Planned | Screenshots, logs, test evidence, portfolio narrative |

## License

TBD — project is currently for personal portfolio use. Do not redistribute
without contacting the owner.

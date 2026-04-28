# Project Overview

**Project:** Body Control Zonal Lighting
**Owner:** Pavankumar Satyanarayan
**Document scope:** non-architectural overview — what the project *is*, what
it is *for*, and what "done" looks like. Architecture, wire formats, and
state machines are in their own documents.

---

## 1. One-sentence mission

Build a portfolio-quality automotive software project that uses a familiar
body-control lighting feature to teach and demonstrate the industry's move
from classic CAN-oriented ECU thinking toward zonal, Ethernet-based,
service-oriented, software-defined-vehicle architecture.

## 2. Why lighting, and why now

Body-control lighting (indicators, hazard, park, headlamps) is an ideal
demonstration feature:

- **Universally recognisable.** Any engineer or interviewer immediately
  understands the behaviour, so the conversation can focus on the
  architecture instead of the domain.
- **Just complex enough.** It has real arbitration (hazard priority over
  indicators), real state (per-lamp on/off), real reporting (per-lamp and
  per-node health), and real timing (heartbeat timeouts).
- **Mirrors the industry transition.** Historically this would be a small
  CAN node with hard-wired logic. The modern answer is a central zonal
  controller driving a rear-zone node over Ethernet with service
  interfaces. Same user-visible feature, very different software story.

## 3. End-state vision

A reviewer opens the repository and, within a few minutes, can:

1. Build the whole system on Linux with a single CMake invocation.
2. Run the four executables and drive a real end-to-end flow: HMI → central
   controller → rear node → status/health → HMI.
3. Read the layered code and see that the feature logic is written once and
   reused, while platform, transport, and service concerns are cleanly
   separated.
4. Read the docs and understand exactly *why* the architecture is shaped
   that way, *how* the service interface is defined, and *what* changes
   between the Linux simulator and the STM32 hardware target.

## 4. Node responsibilities

| Node / executable | Responsibility |
|---|---|
| **Central zone controller** | Receive upstream intent (from HMI or diagnostic console via the operator service), arbitrate command priorities, issue rear-lighting service requests, track node availability and cached state, fan status events back to all operator clients. |
| **Rear lighting node** | Apply commands to outputs (simulated on Linux, GPIO on STM32), publish lamp status and node-health events, reflect failures honestly. Same logical behaviour regardless of platform. |
| **HMI control panel** | Provide an operator-facing interface for lamp commands; surface the latest state via push events from the controller. Available as a Qt6 QML dashboard or a terminal menu. |
| **Diagnostic console** | Engineering entry point for manual requests, health checks, and service-path validation. |

## 5. What the user will see

A minimal but believable slice of an automotive platform:

- A Qt6 GUI (or terminal menu) that sends user actions.
- A controller that never just forwards raw input but always arbitrates.
- A rear node that responds authoritatively with lamp state and health.
- A visible and meaningful node-unavailability signal when the rear node
  stops responding.
- The same logical flow whether the rear node is a Linux process or a
  NUCLEO-H753ZI board with real LEDs.

## 6. What success means

- A stranger on GitHub can read the README and the `doc/` folder and
  understand the project in under five minutes.
- The Linux build is green; CTest is green; the five executables run and
  communicate.
- The STM32 target compiles and runs on a NUCLEO-H753ZI, driving five GPIO
  outputs from lamp commands sent over Ethernet.
- The code reads like automotive-style C++, not a student script collection.
- The story answers the question clearly: *why is the market shifting,
  and how does that change the way this feature is designed?*

## 7. What this project deliberately is **not**

- Not a full AUTOSAR stack. It borrows Adaptive AUTOSAR and SOME/IP
  concepts at the architectural level but does not attempt to reproduce
  them.
- Not an ara::com implementation. The service layer mirrors ara::com
  proxy/skeleton concepts intentionally, but uses vsomeip directly rather
  than a full Adaptive AUTOSAR middleware stack. See
  `doc/system_architecture.md` for the mapping.
- Not a production vsomeip integration. The vsomeip transport is wired and
  functional for the Linux simulation path; the hardware path uses a direct
  UDP adapter for simplicity. A full SOME/IP SD deployment is tracked as
  future work.
- Not safety-certified. MISRA-oriented thinking is applied to style, but
  there is no formal static analysis or certification claim.
- Not a general lighting platform. The scope is intentionally narrow
  because breadth would obscure the architectural point.

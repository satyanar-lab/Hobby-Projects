# Demo Walkthrough — NUCLEO-H753ZI Hardware

This document covers bringing up the body control zonal lighting system on real
hardware: a NUCLEO-H753ZI rear zone node connected via Ethernet to a Linux PC
running the Central Zone Controller and HMI.

---

## 1. Hardware Setup

### Components

| Item | Notes |
|---|---|
| STM32 NUCLEO-H753ZI | Any revision. USB-B mini cable for power and virtual COM port |
| Ethernet cable | Direct PC ↔ NUCLEO (no switch required, auto-MDIX handles crossover) |
| 5× LEDs + 330 Ω resistors | Standard 3 mm or 5 mm LEDs; one per lamp function |
| Breadboard + jumper wires | Connect NUCLEO Morpho pins to LED anodes; cathodes to GND via resistor |

### LED Wiring Table

Connect each LED anode to the listed NUCLEO GPIO, cathode to any GND pin,
with a 330 Ω resistor in series.

| Lamp Function | MCU GPIO | Behavior when active |
|---|---|---|
| Left Indicator | PB5 | Blinks 500 ms ON / 500 ms OFF |
| Right Indicator | PB1 | Blinks 500 ms ON / 500 ms OFF |
| Hazard Lamp | PB2 | Blinks 500 ms ON / 500 ms OFF |
| Park Lamp | PB3 | Steady ON |
| Head Lamp | PB4 | Steady ON |

> **Hazard note:** When hazard is active, PB2 (hazard), PB5 (left indicator),
> and PB1 (right indicator) all blink in phase together. Individual indicator
> commands are blocked while hazard is on.

Pin locations on the NUCLEO-H753ZI Morpho connectors are documented in
ST's user manual UM2408, Table 14 / Figure 7.

### Network Configuration (Static)

| Node | IP Address | UDP Port |
|---|---|---|
| PC (Linux / CZC) | 192.168.0.10 | 41000 (receive) |
| NUCLEO (rear zone node) | 192.168.0.20 | 41001 (receive) |

Assign 192.168.0.10/24 to the Ethernet interface connected to the NUCLEO:

```bash
sudo ip addr add 192.168.0.10/24 dev eth1
sudo ip link set eth1 up
```

Replace `eth1` with the actual interface name (`ip link show` to list).

---

## 2. Software Setup

### Prerequisites

- `arm-none-eabi-gcc` toolchain (10.3-2021.10 or later)
- STM32CubeH7 firmware package (V1.11.x) extracted somewhere local
- STM32CubeProgrammer installed (for flashing the .hex)
- Linux build: GCC/Clang, CMake ≥ 3.21, vsomeip3 installed to `/usr/local`

### Build — STM32 Firmware

```bash
cmake -B build-stm32 \
  -DBODY_CONTROL_LIGHTING_TARGET_PLATFORM=stm32 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi-gcc.cmake \
  -DSTM32CUBEH7_DIR=/path/to/STM32Cube_FW_H7_V1.11.x \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-stm32 -j$(nproc)
```

Output hex: **`build-stm32/body_control_stm32.hex`**

### Build — Linux Applications

```bash
cmake -B build \
  -DBODY_CONTROL_LIGHTING_TARGET_PLATFORM=linux \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build -j$(nproc)
```

### Flash the NUCLEO

Open STM32CubeProgrammer, connect via ST-LINK (USB), and program:

```
build-stm32/body_control_stm32.hex
```

The board will reset automatically after programming.

---

## 3. Four-Terminal Run Sequence

Open four terminal windows before starting.

### Terminal 1 — PuTTY / Serial Monitor

Connect to the NUCLEO virtual COM port **before** powering the board.

| Setting | Value |
|---|---|
| Port | `/dev/ttyACM0` (or `COMx` on Windows) |
| Baud rate | 115200 |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |

Expected output after boot:

```
[INF] Boot: clocks+UART up
[INF] Boot: lwip_init done
[INF] Rear lighting node started
```

Once the Ethernet link comes up (PHY polling detects link within ~1 s):

```
[INF] Transport up
```

### Terminal 2 — Central Zone Controller

```bash
tools/run_controller.sh
```

Or directly (if vsomeip is not needed):

```bash
./build/app/central_zone_controller_app
```

Expected output:

```
Central zone controller is running.
Press Ctrl+C to shut down.
```

The CZC binds UDP port 41000 and begins forwarding every lamp command to both
the vsomeip simulator path (if running) and directly to the NUCLEO at
192.168.0.20:41001.

### Terminal 3 — HMI Control Panel

**Qt6 GUI (recommended):**

```bash
./build/app/hmi_control_panel_qt
```

The Qt6 automotive-style dark window opens.  The controller-online dot in
the top-right corner turns green, and the node health bar at the bottom
shows **ETH UP · SVC UP** within 1–2 seconds as the NUCLEO sends its first
`NodeHealthEvent` via the CZC.

**Terminal fallback (no Qt required):**

```bash
./build/app/hmi_control_panel_terminal
```

### Terminal 4 — (Optional) Rear Lighting Simulator

Only needed if you also want the Linux vsomeip simulation path to respond.
The NUCLEO works without it.

```bash
tools/run_simulator.sh
```

---

## 4. Expected Behavior per HMI Button

| HMI Action | LEDs | PuTTY Output |
|---|---|---|
| Left Indicator (toggle on) | PB5 blinks 500 ms | `[INF] Lamp command applied: ON` |
| Left Indicator (toggle off) | PB5 off | `[INF] Lamp command applied: OFF` |
| Right Indicator (toggle on) | PB1 blinks 500 ms | `[INF] Lamp command applied: ON` |
| Right Indicator (toggle off) | PB1 off | `[INF] Lamp command applied: OFF` |
| Hazard (toggle on) | PB2+PB5+PB1 blink together | `[INF] Hazard: ON` |
| Hazard (toggle off) | All three off | `[INF] Hazard: OFF` |
| Park Lamp (toggle on) | PB3 steady | `[INF] Lamp command applied: ON` |
| Park Lamp (toggle off) | PB3 off | `[INF] Lamp command applied: OFF` |
| Head Lamp (toggle on) | PB4 steady | `[INF] Lamp command applied: ON` |
| Head Lamp (toggle off) | PB4 off | `[INF] Lamp command applied: OFF` |

**Exclusivity rule:** Activating left indicator while right is on (or vice versa)
immediately deactivates the other side. The NUCLEO enforces this locally — the
CZC state is not consulted.

**Indicator blocked during hazard:** Pressing a left/right indicator button
while hazard is active prints `[WRN] Indicator blocked: hazard active` on
PuTTY and has no effect on LEDs.

---

## 5. PuTTY Startup-to-Command Trace

```
[INF] Boot: clocks+UART up
[INF] Boot: lwip_init done
[INF] Rear lighting node started
[INF] Transport up                   ← PHY link detected, LwIP socket bound
... (1 s intervals)
[INF] Lamp command applied: ON       ← first HMI button press
[INF] Lamp command applied: OFF
[INF] Hazard: ON
[INF] Hazard: OFF
[WRN] Indicator blocked: hazard active
```

Each log line is prefixed with `[INF]` (info) or `[WRN]` (warning) by the
`Stm32DiagnosticLogger` and transmitted over USART3 (PD8/PD9) at 115200 baud.

---

## 6. Architecture — Why This Is Zonal SDV, Not a Traditional ECU

### Traditional ECU Architecture

A classic body control module (BCM) is a single, monolithic ECU:
- All lamp outputs are wired directly to the BCM via a physical harness
- Logic and hardware actuation live in the same chip
- Adding a lamp function means re-spinning the BCM firmware and the harness

### This Project's Zonal Architecture

```
 ┌─────────────────────────────┐        Ethernet (UDP)
 │    Linux PC / Head Unit     │ ══════════════════════════════╗
 │                             │                               ║
 │  ┌──────────────────────┐   │                    ┌──────────╨──────────┐
 │  │   HMI Control Panel  │   │                    │  NUCLEO-H753ZI      │
 │  │  (operator intent)   │   │                    │  Rear Zone Node     │
 │  └──────────┬───────────┘   │                    │                     │
 │             │               │                    │  RearLightingFmgr   │
 │  ┌──────────▼───────────┐   │                    │  BlinkManager       │
 │  │ CentralZoneController│ ──┼──── lamp commands ─▶  GpioOutputDriver   │
 │  │ CommandArbitrator    │   │◀─── status events ─── PB1/PB2/PB3/PB4/PB5│
 │  └──────────────────────┘   │                    └─────────────────────┘
 └─────────────────────────────┘
```

**Key design differences:**

| Aspect | Traditional ECU | This Project |
|---|---|---|
| Hardware coupling | ECU firmware = hardware driver | Zone node firmware is thin; domain logic lives on the head unit |
| Communication | Private LIN/CAN bus | Standard Ethernet UDP; same cable used for everything |
| Lamp arbitration | Monolithic state machine in BCM | Distributed: CZC handles vehicle-level policy; zone node handles local safety (exclusivity, hazard blocking) |
| Scaling | New function = new wire + new ECU | New zone = new Ethernet node; no harness re-spin |
| Software updates | Flash each ECU separately over CAN | OTA via Ethernet to both head unit and zone node independently |

**Why "zonal":** The vehicle is divided into physical zones (front-left, front-right, rear,
etc.). Each zone has a dedicated compute node that owns all actuators in that zone and
speaks a standardised network protocol. A central orchestrator (the CZC) holds the
vehicle-level state machine and delegates execution commands without knowing which GPIO
pin each lamp lives on.

This separation allows the software architecture (services, state machines, arbitration)
to evolve independently from the hardware wiring — the defining characteristic of
Software-Defined Vehicle (SDV) designs.

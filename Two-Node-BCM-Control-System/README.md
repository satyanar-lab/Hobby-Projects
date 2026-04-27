# Two-Node BCM Control System

An embedded C project demonstrating a two-node Body Control Module (BCM) system
communicating over CAN FD. One node reads physical switches and acts as the
bus master; the other drives lighting outputs based on received commands.

---

## Hardware

| Item | Detail |
|------|--------|
| Microcontroller | STM32L152RE (STM32L1 series, Cortex-M3, 32 MHz) |
| CAN FD controller | Microchip MCP2517FD (Longan CAN FD board) |
| MCU-to-CAN interface | SPI (SPI1, chip-select on PC7 / PB6) |
| UART diagnostics | USART2 (PA2 TX / PA3 RX, 115 200 baud) |
| Toolchain | STM32CubeIDE (Eclipse/GCC ARM) |
| HAL | STM32CubeMX-generated STM32L1xx HAL |

### BCM Master pin assignments

| Signal | Pin |
|--------|-----|
| HEAD_LAMP_SW | PC4 (active-low) |
| PARK_LAMP_SW | PC0 (active-low) |
| LEFT_IND_SW | PC1 (active-low) |
| RIGHT_IND_SW | PC2 (active-low) |
| HAZARD_SW | PC3 (active-low) |
| CAN_CS | PC7 |
| CAN_INT | PA10 |
| STATUS_LED | PA8 |
| REAR_LINK_LED | PB8 |

### Lighting Node pin assignments

Lighting output LEDs (HEAD, PARK, LEFT, RIGHT, HAZARD, HB_LED) are mapped via
`lighting_node/Core/Inc/main.h` and driven from `io_lighting_outputs.c`.

---

## Features

- **CAN FD with BRS** — arbitration phase at 500 kbps, data phase at 2 Mbps;
  TDC (Transmitter Delay Compensation) auto-enabled via MCP2517FD.
- **Heartbeat supervision** — BCM increments a counter in every 100 ms command
  frame; Lighting Node detects a fresh counter value and resets a 350 ms
  watchdog. Timeout triggers fail-safe mode automatically.
- **XOR checksum** — bytes [0..14] of every 16-byte CAN FD frame are XOR-ed;
  result placed at byte [15]. Receiver rejects frames where the computed
  checksum does not match the received byte.
- **Lamp command arbitration** — hazard overrides individual indicators; left
  and right indicators are mutually exclusive; all are encoded as bit-flags in
  a single `command_bits` byte.
- **UART diagnostics** — `svc_log` service formats timestamped trace lines
  (TX/RX frames + application events) over USART2 at boot and every cycle.
- **Link LED feedback** — BCM master REAR_LINK_LED: off = status timeout,
  solid = lighting node in fail-safe, fast blink (120 ms) = healthy link.

---

## Folder structure

```
Two-Node-BCM-Control-System/
├── bcm_master/                   STM32CubeIDE project — BCM Master Node
│   └── Core/
│       ├── Inc/
│       │   ├── app_bcm.h         Application layer public API + diagnostics struct
│       │   ├── can_test.h        CAN test constants (ID, DLC)
│       │   ├── canfd_spi.h       SPI transport primitives for MCP2517FD
│       │   ├── com_lighting_if.h Encode/decode + checksum API
│       │   ├── io_bcm_inputs.h   Switch debounce + latch state
│       │   ├── lighting_messages.h  Shared frame layouts and bit-flag defines
│       │   ├── main.h            HAL pin definitions (CubeMX-generated)
│       │   ├── mcp2517fd_can.h   MCP2517FD register driver API
│       │   └── svc_log.h         UART log service
│       └── Src/
│           ├── app_bcm.c         Main run-cycle: inputs → CAN TX → RX poll → LED
│           ├── canfd_spi.c       SPI reset, read/write word/bytes
│           ├── com_lighting_if.c Checksum calculation, frame encode/decode
│           ├── io_bcm_inputs.c   30 ms debounce, toggle-latch arbitration
│           ├── main.c            HAL init + superloop calling AppBcm_*
│           ├── mcp2517fd_can.c   Mode control, bit-timing, FIFO config, TX/RX
│           └── svc_log.c         Buffered UART formatter (hex, decimal, trace)
├── lighting_node/                STM32CubeIDE project — Lighting Node
│   └── Core/
│       ├── Inc/
│       │   ├── app_lighting.h    Application layer public API + diagnostics struct
│       │   ├── io_lighting_outputs.h  Output state and apply functions
│       │   └── main.h            HAL pin definitions
│       └── Src/
│           ├── app_lighting.c    Main run-cycle: RX poll → outputs → TX status
│           ├── io_lighting_outputs.c  Fail-safe, command apply, GPIO write
│           ├── main.c            HAL init + superloop calling AppLighting_*
│           └── (shared drivers)  canfd_spi.c, mcp2517fd_can.c, com_lighting_if.c,
│                                 svc_log.c — identical copies in each project
└── tests/                        Host-side unit test stubs (no MCU required)
    ├── test_heartbeat_timeout.c
    ├── test_checksum_validation.c
    ├── test_lamp_arbitration.c
    └── test_runner.c
```

---

## Building and flashing

### Prerequisites

- STM32CubeIDE 1.14 or later
- ST-LINK/V2 or on-board ST-LINK (Nucleo-L152RE)
- Two Nucleo-L152RE boards with Longan CAN FD shields attached

### Steps

1. Open STM32CubeIDE and import both projects:
   - `File → Import → Existing Projects into Workspace`
   - Select `bcm_master/` and `lighting_node/` separately.

2. Build each project:
   - Right-click project → `Build Project` (or `Ctrl+B`).
   - Ensure zero errors in the Console view.

3. Flash BCM Master:
   - Connect the first Nucleo board via USB.
   - Right-click `bcm_master` → `Run As → STM32 Cortex-M C/C++ Application`.

4. Flash Lighting Node:
   - Connect the second Nucleo board via USB.
   - Right-click `lighting_node` → `Run As → STM32 Cortex-M C/C++ Application`.

5. Connect the two CAN FD boards (CANH–CANH, CANL–CANL, GND–GND) and ensure
   at least one 120 Ω termination resistor is present on the bus.

6. Open a serial terminal (115 200 8N1) on each board's USB-UART port to watch
   the `svc_log` trace output.

### Running host-side unit tests

```bash
cd Two-Node-BCM-Control-System/tests
gcc -I../bcm_master/Core/Inc \
    test_runner.c \
    test_heartbeat_timeout.c \
    test_checksum_validation.c \
    test_lamp_arbitration.c \
    ../bcm_master/Core/Src/com_lighting_if.c \
    ../bcm_master/Core/Src/io_bcm_inputs.c \
    -o run_tests && ./run_tests
```

The stubs compile and link against the real application code; stub out any
HAL calls (e.g. `HAL_GetTick`, `HAL_GPIO_ReadPin`) with lightweight shims
in `test_runner.c`.

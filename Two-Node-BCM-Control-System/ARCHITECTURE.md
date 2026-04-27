# Architecture — Two-Node BCM Control System

---

## System overview

```
  ┌─────────────────────────────┐          ┌─────────────────────────────┐
  │        BCM MASTER           │          │       LIGHTING NODE         │
  │       (Node ID = 1)         │          │       (Node ID = 2)         │
  │                             │          │                             │
  │  GPIO switches (active-low) │          │  GPIO LED outputs           │
  │  HEAD / PARK / LEFT /       │          │  HEAD / PARK / LEFT /       │
  │  RIGHT / HAZARD             │          │  RIGHT / HAZARD / HB_LED    │
  │                             │          │                             │
  │  io_bcm_inputs              │          │  io_lighting_outputs        │
  │  (debounce + latch)         │          │  (fail-safe + apply-cmd)    │
  │         │                   │          │         ▲                   │
  │         ▼                   │  CAN FD  │         │                   │
  │  app_bcm ──────────────────►│──0x123──►│ app_lighting               │
  │  (100 ms TX cycle)          │          │  (poll RX, update outputs,  │
  │         ◄──────────────────◄│──0x124──◄│   100 ms status TX)        │
  │  (poll RX, link LED)        │          │                             │
  │                             │          │                             │
  │  svc_log → USART2           │          │  svc_log → USART2           │
  └─────────────────────────────┘          └─────────────────────────────┘
          MCP2517FD via SPI                        MCP2517FD via SPI
```

Both nodes run a bare-metal superloop (no RTOS). `main.c` calls `Init`,
`Start`, then loops calling `RunCycle` indefinitely.

---

## Node roles

### BCM Master

Responsibility: read the driver's switch panel, encode a command, and
supervise the link to the lighting node.

Key timings:
| Constant | Value | Purpose |
|----------|-------|---------|
| `APP_BCM_TX_PERIOD_MS` | 100 ms | Command transmit interval |
| `APP_BCM_STATUS_TIMEOUT_MS` | 500 ms | Max gap before declaring link lost |
| `APP_BCM_LINK_LED_BLINK_MS` | 120 ms | Toggle interval for healthy-link blink |
| `APP_BCM_SUMMARY_PERIOD_MS` | 500 ms | Periodic UART diagnostic summary |

Run-cycle order (`AppBcm_RunCycle`):
1. `IoBcmInputs_Process` — sample GPIOs, debounce, update latches.
2. `AppBcm_SendCommand` — if 100 ms elapsed, encode frame and transmit on 0x123.
3. `AppBcm_PollStatus` — drain RX FIFO2 for frames with SID 0x124.
4. `AppBcm_UpdateStatusTimeout` — set `status_timeout` if last valid RX > 500 ms ago.
5. `AppBcm_UpdateLinkLed` — drive REAR_LINK_LED per link health.
6. `AppBcm_ProcessLogs` — emit change-triggered and periodic UART traces.
7. `AppBcm_UpdateDiag` — mirror internal state into `g_app_bcm_diag`.

### Lighting Node

Responsibility: receive commands from the BCM, supervise the heartbeat, drive
LED outputs, and report status back.

Key timings:
| Constant | Value | Purpose |
|----------|-------|---------|
| `APP_LIGHTING_HEARTBEAT_TIMEOUT_MS` | 350 ms | Max gap between fresh heartbeats |
| `APP_LIGHTING_HEARTBEAT_LED_MS` | 250 ms | HB LED toggle interval |
| `APP_LIGHTING_INDICATOR_MS` | 330 ms | Turn-indicator blink period |
| `APP_LIGHTING_STATUS_TX_MS` | 100 ms | Status transmit interval |
| `APP_LIGHTING_SUMMARY_MS` | 500 ms | Periodic UART diagnostic summary |

Run-cycle order (`AppLighting_RunCycle`):
1. `AppLighting_PollCommand` — drain RX FIFO2 for frames with SID 0x123.
2. `AppLighting_UpdateTiming` — check heartbeat watchdog, toggle HB LED and
   indicator phase, apply command or fail-safe to output state, write GPIOs.
3. `AppLighting_SendStatus` — if 100 ms elapsed, build and transmit 0x124.
4. `AppLighting_ProcessLogs` — emit change-triggered and periodic UART traces.
5. `AppLighting_UpdateDiag` — mirror internal state into `g_app_lighting_diag`.

---

## CAN FD frame structure

Both frames are 16 bytes (DLC = 0xA, FDF = 1, BRS = 1) using standard 11-bit IDs.

### BCM Command frame — SID 0x123

| Byte | Field | Notes |
|------|-------|-------|
| 0 | `command_bits` | Bit-flags: HEAD=0x01, PARK=0x02, LEFT=0x04, RIGHT=0x08, HAZARD=0x10 |
| 1 | `heartbeat_counter` | Incremented each TX; Lighting Node compares to last received value |
| 2 | `sender_node_id` | Fixed = 1 |
| 3 | `bcm_alive` | Fixed = 1 |
| 4 | `rolling_counter` | LSB of `tx_count`; wraps 0–255 |
| 5–14 | `reserved_5`–`reserved_14` | Patterned 0x11–0xAA (future use / bus-load padding) |
| 15 | `checksum` | CRC-8/SAE-J1850 of bytes [0..14] |

### Lighting Status frame — SID 0x124

| Byte | Field | Notes |
|------|-------|-------|
| 0 | `flags` | HEAD_ACT=0x01, PARK_ACT=0x02, LEFT_ACT=0x04, RIGHT_ACT=0x08, HAZARD_ACT=0x10, HB_TIMEOUT=0x20, FAIL_SAFE=0x40 |
| 1 | `state` | Lighting state enum (INIT=0 … FAIL_SAFE_PARK=7) |
| 2 | `last_heartbeat_rx` | Last heartbeat counter value accepted |
| 3 | `status_counter` | LSB of `status_tx_count` |
| 4 | `command_bits_echo` | Echoes the received `command_bits` |
| 5 | `rx_count_lsb` | LSB of command receive counter |
| 6 | `heartbeat_led_state` | 0/1 reflecting HB LED GPIO |
| 7 | `indicator_blink_state` | 0/1 current indicator phase |
| 8–12 | Per-lamp echo | head, park, left, right, hazard (0/1 from command_bits) |
| 13 | `diagnostic_pattern_a5` | Fixed = 0xA5 (integrity marker) |
| 14 | `node_id` | Fixed = 2 |
| 15 | `checksum` | CRC-8/SAE-J1850 of bytes [0..14] |

---

## BRS (Bit Rate Switching)

MCP2517FD is configured in Normal CAN FD mode with BRS enabled:

- **Nominal phase** (arbitration): 500 kbps — `MCP2517FD_CAN_SetNominalBitTiming_500k_20MHz`
- **Data phase**: 2 Mbps — `MCP2517FD_CAN_SetDataBitTiming_2M_20MHz`
- **TDC**: auto mode — `MCP2517FD_CAN_EnableTdcAuto` (compensates SPI-path delay at 2 Mbps)
- **BRS bit**: set in TX object header — `MCP2517FD_CAN_EnableBRS` sets the BRS flag in C1CON;
  `MCP2517FD_CAN_TxFifo1SendFd16` is called with `brs = 1`.

All frames switch to the faster data rate immediately after the BRS bit in the
arbitration field, then return to nominal rate for the CRC delimiter and ACK.

---

## Heartbeat state machine

The heartbeat mechanism provides liveness supervision without a separate
protocol message. The BCM Master embeds an 8-bit free-running counter
(`heartbeat_counter`, incremented every TX cycle) into every command frame.

```
Lighting Node state:

  heartbeat_timeout = 1          heartbeat_timeout = 0
  fail_safe = 1                  fail_safe = 0
  outputs → FAIL_SAFE_PARK       outputs → commanded state
        │                                │
        │  valid frame received          │  > 350 ms since last
        │  AND counter changed           │  counter change
        └───────────────────────────────►│◄──────────────────┘
                                          (timer checked each cycle)
```

Detection logic (`AppLighting_PollCommand`):
- A frame passes checksum validation.
- `command_frame.heartbeat_counter != last_heartbeat` — counter advanced.
- `last_heartbeat_tick` is updated; `heartbeat_timeout` and `fail_safe` cleared.

Timeout logic (`AppLighting_UpdateTiming`):
- `(now_ms - last_heartbeat_tick) > 350` — sets `heartbeat_timeout = 1`,
  `fail_safe = 1`, resets `indicator_phase`, calls `IoLightingOutputs_ApplyFailSafe`.

Fail-safe output state: park lamp ON, all others OFF (`LIGHTING_STATE_FAIL_SAFE_PARK = 7`).

---

## Checksum validation path

```
Transmitter (encode):                 Receiver (decode):

  data[0..14] populated               data[0..14] received over CAN
        │                                     │
        ▼                                     ▼
  checksum = CRC-8/SAE-J1850          rx_checksum  = data[15]
             (data[0..14])            calc_checksum = CRC-8/SAE-J1850
  data[15] = checksum                              (data[0..14])
        │                                     │
        ▼                                     ▼
  MCP2517FD_CAN_TxFifo1SendFd16       if (rx_checksum == calc_checksum)
                                          → unpack frame fields
                                          → valid_frame = TRUE
                                      else
                                          → discard, increment fail_count
                                          → checksum_error flag set
```

Implementation: `ComLighting_CalculateCRC8` (com_lighting_if.c) uses
CRC-8/SAE-J1850 (poly=0x1D, init=0xFF, final_xor=0xFF, no reflection),
satisfying ISO 26262 ASIL-B diagnostic coverage requirements. Called from
both `ComLighting_EncodeBcmCommand` / `ComLighting_EncodeLightingStatus`
on TX and `ComLighting_DecodeBcmCommand` / `ComLighting_DecodeLightingStatus`
on RX.

---

## Lamp command arbitration

Priority rules encoded in `IoLightingOutputs_ApplyCommand` and
`IoBcmInputs_Process`:

1. **Hazard** (bit 0x10) — overrides all indicators; forces park lamp ON;
   left and right indicators blink together at `indicator_phase`.
   Activating hazard clears left/right latches on the BCM side.
2. **Left indicator** (bit 0x04) — mutually exclusive with right; activating
   left clears the right latch.
3. **Right indicator** (bit 0x08) — mutually exclusive with left.
4. **Head lamp** (bit 0x01) and **Park lamp** (bit 0x02) — independent toggles,
   no mutual exclusion.
5. State reporting priority (highest wins): HAZARD > LEFT > RIGHT > HEAD > PARK > NORMAL.

Switch inputs are toggle-latched: a press event flips the latched state,
so the command bit remains set until the switch is pressed again.

---

## MCP2517FD driver layers

```
app_bcm / app_lighting
       │
       ▼
mcp2517fd_can.c   — register-level driver (mode, bit-timing, FIFO, TX/RX objects)
       │
       ▼
canfd_spi.c       — SPI transport (reset, read/write word/bytes, CS control)
       │
       ▼
STM32 HAL SPI     — HAL_SPI_TransmitReceive
```

`canfd_spi.c` manages the SPI CS line and the MCP2517FD SPI command encoding
(address + command byte prefix). `mcp2517fd_can.c` builds 32-bit register
words and TX object headers according to the MCP2517FD datasheet.

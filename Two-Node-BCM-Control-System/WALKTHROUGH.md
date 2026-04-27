# Code Walkthrough — Two-Node BCM Control System

This document walks through the project in the order a new reader should
follow it: shared definitions first, then the driver stack, then each
application node.

---

## 1. Start with the shared message contract

**File:** `bcm_master/Core/Inc/lighting_messages.h`

This header is the contract between both nodes. Read it before anything else.

- `LIGHTING_BCM_COMMAND_ID = 0x123` — CAN SID for BCM → Lighting.
- `LIGHTING_STATUS_ID = 0x124` — CAN SID for Lighting → BCM.
- `LIGHTING_FRAME_LENGTH = 16` — every frame is exactly 16 bytes; DLC maps to 0xA in CAN FD.
- `LIGHTING_FRAME_CHECKSUM_INDEX = 15` — byte [15] is always the checksum.
- Bit-flag defines for `command_bits` (HEAD=0x01 … HAZARD=0x10) and `flags`
  byte in the status frame.
- Two struct typedefs: `Lighting_BcmCommandFrame_t` and `Lighting_StatusFrame_t`.
  Note how fields map directly to byte positions — the encode/decode functions
  rely on this one-to-one mapping.

---

## 2. Understand the checksum layer

**File:** `bcm_master/Core/Src/com_lighting_if.c`

`ComLighting_CalculateChecksum` (line 4): XOR-folds bytes [0..14].
Simple, deterministic, zero-dependency.

`ComLighting_EncodeBcmCommand` (line 22): copies struct fields into a flat
byte array in field order, then appends the checksum at [15]. The caller
can then inspect `p_data[15]` to cache the transmitted checksum.

`ComLighting_DecodeBcmCommand` (line 48): reads `p_data[15]` as `rx_checksum`,
computes `calc_checksum` over [0..14], compares them. Only if they match does
it unpack the struct fields. This ensures corrupt frames are silently dropped
and the counters (`checksum_ok_count`, `checksum_fail_count`) track integrity.

The status frame encode/decode (`ComLighting_EncodeLightingStatus`,
`ComLighting_DecodeLightingStatus`) follow the identical pattern for the
reverse direction.

`ComLighting_IsFlagSet` (line 154): one-liner helper used throughout the
application layer to test individual bits in the `flags` byte without
masking boilerplate in every caller.

---

## 3. Trace the SPI driver

**File:** `bcm_master/Core/Src/canfd_spi.c` → `mcp2517fd_can.c`

`CANFD_SPI_Reset` asserts a SPI reset command. `CANFD_SPI_ReadWord` and
`CANFD_SPI_WriteWord` wrap single 32-bit register accesses using the
MCP2517FD SPI command format (command byte + 12-bit address). All SPI calls
go through STM32 HAL `HAL_SPI_TransmitReceive`.

`mcp2517fd_can.c` builds on top of `canfd_spi.c`:

- `MCP2517FD_CAN_RequestMode` — writes REQOP bits in C1CON, then polls OPMOD
  bits until the controller confirms the new mode.
- `MCP2517FD_CAN_SetNominalBitTiming_500k_20MHz` and
  `MCP2517FD_CAN_SetDataBitTiming_2M_20MHz` — write C1NBTCFG and C1DBTCFG
  with precomputed values for a 20 MHz oscillator.
- `MCP2517FD_CAN_EnableTdcAuto` — enables automatic Transmitter Delay
  Compensation required at 2 Mbps data rate.
- `MCP2517FD_CAN_EnableBRS` — sets the BRS enable bit in C1CON so the
  controller switches bit rate during the data phase.
- `MCP2517FD_CAN_ConfigureTxFifo1_16B` — configures FIFO1 as a TX FIFO with
  16-byte payload size.
- `MCP2517FD_CAN_ConfigureRxFifo2_16B_TS` — configures FIFO2 as an RX FIFO
  with 16-byte payload and timestamp.
- `MCP2517FD_CAN_ConfigureFilter0_Std11_ToFifo2` — sets filter 0 to pass only
  the target SID to FIFO2 (BCM filters for 0x124; Lighting filters for 0x123).
- `MCP2517FD_CAN_TxFifo1SendFd16` — writes a TX object header (DLC=0xA,
  FDF=1, BRS=brs) followed by the 16 data bytes into FIFO1 RAM, then sets
  UINC and TXREQ bits to trigger transmission.
- `MCP2517FD_CAN_RxFifo2PollFd16` — reads FIFO2 status; if `RXIF` set,
  reads the RX object header and 16 data bytes, extracts FDF/BRS/DLC/SID,
  sets UINC to advance the tail pointer.

---

## 4. BCM Master application

**File:** `bcm_master/Core/Src/app_bcm.c`

### 4a. Initialization path

`AppBcm_Init` (called from `main.c` before the loop) calls
`AppBcm_InitContext` which zeroes all fields and seeds sentinel values
(`0xFF`) into the "last logged" fields so the first real values always
trigger a log event.

`AppBcm_Start` (called once after init) initialises `svc_log` then calls
`AppBcm_ConfigureController`:

1. SPI reset + 35 ms settle.
2. Read OSC register (0x0E00); bit 10 (`OSCRDY`) must be set — proves the
   20 MHz oscillator is running. If not set, `node_ready` stays 0 and
   `RunCycle` becomes a no-op.
3. Configure mode → Config, set both bit-timings, enable TDC + BRS,
   configure TX FIFO1 (16B) and RX FIFO2 (16B+TS), set filter 0 to pass
   SID 0x124, request Normal FD mode.

### 4b. Input debouncing (`io_bcm_inputs.c`)

`IoBcmInputs_Process` runs every cycle:

- `IoBcmInputs_ReadActiveLow` reads each GPIO — pin LOW → logical 1 (pressed).
- `IoBcmInputs_UpdateDebounced`: if the raw sample differs from the stored raw
  value, reset the debounce timer. Only promote to `stable` after 30 ms of
  stability. Returns a one-shot `pressed_event` on the rising edge of `stable`.
- `head_latched` and `park_latched` simply toggle on each pressed event.
- `hazard_latched`: pressing hazard while off clears left/right latches then
  sets hazard. Pressing again clears hazard.
- Left/right debouncing is skipped (outputs discarded) while hazard is active,
  preventing spurious latch changes.
- `IoBcmInputs_GetCommandBits` builds the `command_bits` byte from the five
  latched booleans.

### 4c. Command transmission

`AppBcm_SendCommand` runs every cycle. The 100 ms gate:

```c
if ((now_ms - g_bcm_ctx.last_tx_tick) >= APP_BCM_TX_PERIOD_MS)
```

Steps inside:
1. Call `IoBcmInputs_GetCommandBits` to get latest switch state.
2. Increment `heartbeat_counter` (wraps at 255 → 0).
3. Update `rolling_counter` from LSB of `tx_count`.
4. Call `ComLighting_EncodeBcmCommand` → fills `payload[16]` + checksum.
5. Call `MCP2517FD_CAN_TxFifo1SendFd16(0x123, payload, 1)` — `brs=1`.
6. Log the TX trace via `SvcLog_WriteTrace`.

### 4d. Status reception

`AppBcm_PollStatus` polls FIFO2 for a frame with SID 0x124 and FDF=1.
If found, calls `ComLighting_DecodeLightingStatus`; on checksum pass,
clears `status_timeout` and updates `status_last_valid_tick`.

`AppBcm_UpdateStatusTimeout` runs after the poll: if
`(now_ms - status_last_valid_tick) > 500` sets `status_timeout = 1`.

### 4e. Link LED logic

Three cases (checked in order):
1. `status_timeout == 1` → LED off.
2. Fail-safe flag set in status frame → LED solid on.
3. Otherwise → toggle every 120 ms (blink = healthy link).

---

## 5. Lighting Node application

**File:** `lighting_node/Core/Src/app_lighting.c`

### 5a. Initialization

`AppLighting_InitContext`: starts with `heartbeat_timeout = 1` and
`fail_safe = 1` (safe default on cold boot). `IoLightingOutputs_ApplyFailSafe`
is called immediately — park lamp ON from the very first cycle.

`AppLighting_ConfigureController`: identical to BCM side, except the RX
filter is set to pass SID 0x123.

### 5b. Command reception

`AppLighting_PollCommand` polls FIFO2 for SID 0x123, FDF=1. On checksum pass:

- Increment `command_checksum_ok_count`, clear `command_checksum_error`.
- Compare `command_frame.heartbeat_counter` to `last_heartbeat`.
  If they differ → **heartbeat is fresh**: update `last_heartbeat_tick`,
  clear `heartbeat_timeout` and `fail_safe`.

If checksum fails: increment fail count, set `command_checksum_error`.

Note: a valid frame with the same counter value (retransmit or duplicate) does
**not** refresh the heartbeat timer — only a new counter value counts.

### 5c. Timing and output update

`AppLighting_UpdateTiming` is the heart of the lighting state machine:

**Timeout branch** (`now_ms - last_heartbeat_tick > 350`):
- Sets `heartbeat_timeout = 1`, `fail_safe = 1`, `indicator_phase = 0`.
- Calls `IoLightingOutputs_ApplyFailSafe` → park ON, others OFF,
  `state = LIGHTING_STATE_FAIL_SAFE_PARK`.

**Normal branch**:
- Heartbeat LED toggles every 250 ms (independent of indicator).
- Indicator `phase` toggles every 330 ms — passed to `ApplyCommand` to gate
  the left/right/hazard blinking.
- `IoLightingOutputs_ApplyCommand` evaluates the command_bits:
  - Hazard active → park ON, left+right+hazard blink with `indicator_phase`.
  - No hazard → park/left/right/head evaluated individually; left and right
    are mutex (see arbitration in section 4b).
  - State enum set for the highest-priority active function.

`IoLightingOutputs_Write` translates the output struct to GPIO calls.

### 5d. Status transmission

Every 100 ms: build `Lighting_StatusFrame_t` from current output and
diagnostic state, encode via `ComLighting_EncodeLightingStatus`, transmit
on 0x124 with BRS.

---

## 6. UART diagnostics

**File:** `bcm_master/Core/Src/svc_log.c`

`SvcLog_Init` stores the UART handle. All subsequent calls are polling
(blocking transmit), appropriate for a diagnostic-only path.

`SvcLog_WriteTrace` formats a standard trace line:
```
<timestamp_ms> <channel> <direction> SID=<hex> DLC=<dec> FD=<0/1> BRS=<0/1> DATA=<hex bytes>
```

Application events use the `SvcLog_Buffer_t` pattern: init → append fields →
transmit. The buffer is stack-allocated (256 bytes) and re-used each call.

Change-triggered logging (in `AppBcm_ProcessLogs` / `AppLighting_ProcessLogs`)
compares current values to `last_logged_*` shadow fields; only emits a message
when the value changes. Periodic summary lines fire every 500 ms regardless.

---

## 7. Reading the diagnostics at runtime

Connect a terminal (115 200 8N1). Typical startup on BCM Master:

```
================================================================================
1234 APP BCM BOOT SPI=1 FDMODE=0
================================================================================
1334 APP BCM SUM CMD=00 HB=0 TX=0 TO=1 ST=0 FAIL=0 CSERR=0 OK=0 FAILCS=0
```

After the lighting node connects and starts sending status:

```
================================================================================
1450 APP BCM LIGHT STATUS RESTORED ST=1 CNT=1
================================================================================
1450 APP BCM LIGHT ST=1 FAIL=0 HB_TO=0 H=0 P=0 L=0 R=0 Z=0
```

Pressing HEAD_LAMP_SW:

```
================================================================================
1600 APP BCM CMD H=1 P=0 L=0 R=0 Z=0 HB=16 CS=AB
================================================================================
1700 APP BCM LIGHT ST=2 FAIL=0 HB_TO=0 H=1 P=0 L=0 R=0 Z=0
```

If the CAN cable is disconnected, within 500 ms on BCM Master:

```
================================================================================
2200 APP BCM LIGHT STATUS TIMEOUT
```

And within 350 ms on Lighting Node:

```
================================================================================
2100 APP LIGHT HEARTBEAT TIMEOUT
================================================================================
2100 APP LIGHT STATE=7 FAIL=1 H=0 P=1 L=0 R=0 Z=0
```

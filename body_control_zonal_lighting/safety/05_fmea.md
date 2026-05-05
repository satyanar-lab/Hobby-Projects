# Failure Modes and Effects Analysis (FMEA)

**Reference: ISO 26262-9 / AIAG FMEA 4th edition**

Component-level FMEA for the exterior lighting actuator node. RPN = Severity × Occurrence × Detection (scale 1–10 each). Higher RPN = higher priority for corrective action.

Detection score convention: 1 = almost certain detection; 10 = no detection mechanism.

---

## FMEA Table

| ID | Component | Failure Mode | Effect on Item Function | Detection Mechanism | S | O | D | RPN | Status |
|---|---|---|---|---|---|---|---|---|---|
| FM-001 | Ethernet PHY | No link (cable fault, PHY failure) | Node unreachable; all lamps hold last commanded state indefinitely | NodeHealthMonitor heartbeat timeout at CZC (2 s) | 8 | 2 | 2 | **32** | Implemented |
| FM-002 | GPIO output driver | Stuck high (output latches on) | Lamp cannot be turned off regardless of command | None — no current sensing on outputs | 6 | 2 | 10 | **120** | **Gap — highest priority** |
| FM-003 | Application thread | Stack overflow in any Zephyr thread | Hard fault → ECU reset → all lamp outputs off (power-on safe state) | `CONFIG_HW_STACK_PROTECTION=y` + MPU stack guard | 8 | 2 | 3 | **48** | Implemented |
| FM-004 | Flash (slot 0) | Corruption of application image | ECU fails to boot or boots corrupted firmware; unpredictable GPIO state | MCUboot ECDSA-P256 signature validation on every boot | 10 | 2 | 2 | **40** | Implemented |
| FM-005 | Ethernet stack | Network packet flood / DoS | `g_lamp_cmd_queue` fills; new commands dropped; lamps unresponsive to driver | None — no rate limiting or source address filtering | 4 | 3 | 8 | **96** | **Gap** |
| FM-006 | Power supply | Brown-out during lamp transition | GPIO output state unknown at reset; all outputs return to off at next boot | Brown-out reset hardware (MCU `BOR` hardware block) | 4 | 3 | 4 | **48** | Implemented (hardware) |
| FM-007 | OTA block sequencing | UDS sequence counter wraparound | OTA session could accept blocks out of order or re-accept an old session | `OtaHandler` block counter check; `RequestTransferExit` CRC-32 | 8 | 1 | 2 | **16** | Implemented |
| FM-008 | SOME/IP parser | Malformed message accepted as valid command | Arbitrary lamp state written from attacker-controlled payload | `SomeipMessageParser::IsSetLampCommandRequest` + payload size check | 5 | 2 | 3 | **30** | Implemented |
| FM-009 | MCUboot (slot 1) | Rollback to previous image after failed OTA | Node runs old firmware; new safety fixes not applied | MCUboot `BOOT_UPGRADE_TEST` + `boot_write_img_confirmed` call after first successful health TX | 6 | 2 | 2 | **24** | Implemented |
| FM-010 | Hardware watchdog | Absent (not implemented) | Firmware hang leaves GPIO in last state with no automatic recovery | CZC heartbeat timeout detects node silence but node itself does not reset | 8 | 2 | 7 | **112** | **Gap** |

---

## Analysis Notes

### FM-002 — GPIO output stuck high (RPN 120, highest priority gap)

A lamp relay driver output latching on cannot be detected by the software in
the current implementation. `g_lamp_mgr.ApplyCommand` writes state into the
function manager and calls `gpio_.WriteLampOutput`, but never reads the output
back. Production lamp driver ICs typically provide a diagnostic feedback pin
(overcurrent sense, open-load detect) — connecting this to an ADC channel and
comparing commanded vs. sensed state would reduce the Detection score from 10
to approximately 2, reducing RPN to ~24.

Until output sensing is implemented, the DTC codes `0xB001`–`0xB005` remain
simulation-only (injected via UDS RoutineControl for test purposes, not raised
autonomously).

**Recommended action:** Add GPIO input feedback from driver IC diagnostic pin;
implement autonomous DTC raise in `FaultManager::InjectFault` when stuck-on is
detected.

---

### FM-005 — Network packet flood / DoS (RPN 96)

The UDP receive path in `ZephyrUdpTransportAdapter` accepts all datagrams on
port 41001 regardless of source address or arrival rate. `k_msgq_put` with
`K_NO_WAIT` will drop overflow messages, so the node will not crash, but
`HealthThread` runs on a lower-priority basis than `UdpRxThread` — a flood
sufficient to saturate the message queue loop could delay health publishing,
eventually triggering the CZC heartbeat timeout.

**Recommended action:** Add IP source address filter (accept only CZC IP
192.168.0.10) and a receive-rate limiter (token bucket, 100 msg/s budget).

---

### FM-010 — Absent hardware watchdog (RPN 112)

The Zephyr IWDG (independent watchdog) driver is not configured in `prj.conf`.
If all five application threads (udp_rx, cmd, blink, health, doip) block
simultaneously due to a kernel bug or deadlock, the MCU will not self-reset.
The CZC will detect the silent node within 2 s (`kNodeHeartbeatTimeout`), but
the rear node itself will remain stuck.

**Recommended action:** Enable `CONFIG_WATCHDOG=y` + `CONFIG_IWDG_STM32=y` in
`prj.conf`; add a watchdog kick from `HealthThread` once per health-publish
interval (every 1000 ms, well within a 2000 ms watchdog window).

---

### FM-003 — Stack overflow (RPN 48, implemented)

`CONFIG_HW_STACK_PROTECTION=y` in `app/zephyr_nucleo_h753zi/prj.conf` enables
the Cortex-M7 MPU to place a guard region at the bottom of each thread stack.
Any write to the guard region generates a MemManage fault, which Zephyr handles
by printing a kernel oops and halting the offending thread. The MCU will
eventually be reset by the absent hardware watchdog (FM-010 gap), so the safe
state dependency between FM-003 and FM-010 is noted.

---

### FM-007 — UDS sequence counter wraparound (RPN 16, implemented)

`OtaHandler` (`src/application/ota_handler.cpp`) tracks the expected block
sequence counter. On `RequestTransferExit` (UDS 0x37), the CRC-32 of all
received blocks is compared against the value provided by the client. MCUboot's
ECDSA-P256 signature validation on the next boot provides a second independent
integrity check — an attacker would need to forge both the CRC and the private
key to install malicious firmware.

---

## RPN Priority Order

| Priority | ID | RPN | Action |
|---|---|---|---|
| 1 | FM-002 | 120 | Add GPIO output current sensing |
| 2 | FM-010 | 112 | Enable hardware IWDG watchdog |
| 3 | FM-005 | 96 | Add source-IP filter + rate limiter |
| 4 | FM-003 | 48 | Already mitigated (HW stack protection) |
| 5 | FM-006 | 48 | Already mitigated (hardware BOR) |
| 6 | FM-004 | 40 | Already mitigated (MCUboot ECDSA) |
| 7 | FM-001 | 32 | Already mitigated (heartbeat timeout) |
| 8 | FM-008 | 30 | Already mitigated (parser size checks) |
| 9 | FM-009 | 24 | Already mitigated (OTA confirm call) |
| 10 | FM-007 | 16 | Already mitigated (CRC + signature) |

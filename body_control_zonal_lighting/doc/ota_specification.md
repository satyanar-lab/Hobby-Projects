# OTA Firmware Update Specification

## Overview

Phase 12 adds an OTA (Over-The-Air) firmware update channel to the body
control rear lighting node.  The transfer protocol follows ISO 14229-1 §14
(RequestDownload / TransferData / RequestTransferExit) delivered over the
existing DoIP TCP diagnostic channel (ISO 13400-2, port 13400).

---

## OTA Sequence

```
Tester / OTA Client                     Rear Lighting Node
─────────────────────                   ──────────────────
1. DoIP RoutingActivation ─────────────►
                          ◄─────────────  RoutingActivationResponse (0x10)

2. 0x10 ExtendedSession   ─────────────►
                          ◄─────────────  0x50 0x03 (session accepted)

3. 0x34 RequestDownload   ─────────────►  Enter OTA mode
         DFI=0x00                         F102 health_state → 0x04 (UPDATING)
         ALFID=0x44                        Open staging file
         addr=0x00000000
         size=<N bytes>
                          ◄─────────────  0x74 0x20 0x02 0x02
                                          (maxBlockLength = 514)

4. 0x36 TransferData      ─────────────►  Write block to staging file
         blockSeq=0x01                    Update running CRC-32
         data[0..511]
                          ◄─────────────  0x76 0x01

   ... repeat for each block (seq wraps 0xFF→0x00) ...

5. 0x37 RequestTransferExit ───────────►  Validate:
         CRC32[3..0] (big-endian)           - received_size == expected_size
                          ◄─────────────   - CRC-32 matches
                                  0x77     fsync + rename to validated path
                                           F102 health_state → 0x01 (OPERATIONAL)

6. Resume normal lamp operations
```

---

## UDS Service Mapping

| Service | SID | Direction | Payload |
|---|---|---|---|
| RequestDownload | 0x34 | Tester → ECU | DFI + ALFID + address (4 B) + size (4 B) |
| RequestDownload response | 0x74 | ECU → Tester | 0x20 + maxBlockLength (2 B) |
| TransferData | 0x36 | Tester → ECU | blockSequenceCounter + data (≤512 B) |
| TransferData response | 0x76 | ECU → Tester | blockSequenceCounter echo |
| RequestTransferExit | 0x37 | Tester → ECU | CRC-32 (4 B, big-endian, optional) |
| RequestTransferExit response | 0x77 | ECU → Tester | — |

---

## Block Size

- **Max data bytes per 0x36 request:** 512
- **maxNumberOfBlockLength** (advertised in 0x74): 514 (= 512 + SID byte + blockSeqCtr byte)
- **ALFID field:** 0x44 (4-byte memory address + 4-byte memory size)
- **Block sequence counter:** starts at 0x01, increments per block, wraps 0xFF → 0x00

---

## CRC Algorithm

**CRC-32/ISO-HDLC** (also known as CRC-32b, used in Ethernet FCS and ZIP):

| Parameter | Value |
|---|---|
| Polynomial (reflected) | 0xEDB88320 |
| Init value | 0xFFFFFFFF |
| Input/output reflection | Yes |
| Final XOR | 0xFFFFFFFF |

Computation: incremental per block as each 0x36 arrives, finalised at 0x37
time by XOR-ing with 0xFFFFFFFF.  The Python client uses `zlib.crc32()` which
implements the same algorithm.

---

## NodeHealthStatus During OTA

The F102 `NodeHealth` DID (byte 0) transitions as follows:

| State | Value | When |
|---|---|---|
| kOperational | 0x01 | Normal operation, no faults |
| kDegraded    | 0x02 | (reserved for future use) |
| kFaulted     | 0x03 | One or more active DTCs |
| kUpdating    | 0x04 | OTA session active (0x34 received, 0x37 not yet) |

The SOME/IP operational health event continues broadcasting at its normal
1-second rate during OTA.  The diagnostic F102 read returns kUpdating while
`OtaSessionManager::IsOtaModeActive()` is true.

---

## File Paths (Linux Simulator)

| Path | Purpose |
|---|---|
| `/tmp/ota_firmware_staging.bin` | Staging file — blocks appended here as 0x36 arrives |
| `/tmp/ota_firmware_validated.bin` | Final image — promoted from staging on successful 0x37 |

The staging file is created with `O_TRUNC` on each new 0x34, ensuring a clean
state regardless of prior partial transfers.  It is `fsync`-ed and atomically
`rename`-ed to the validated path only after CRC and size validation pass.

---

## Error Handling

| Condition | NRC | Code |
|---|---|---|
| 0x34 while OTA already active | uploadDownloadNotAccepted | 0x70 |
| Short/malformed 0x34 request | requestOutOfRange | 0x31 |
| Image size zero or > 10 MiB | requestOutOfRange | 0x31 |
| 0x36/0x37 without prior 0x34 | conditionsNotCorrect | 0x22 |
| Wrong blockSequenceCounter | wrongBlockSequenceCounter | 0x73 |
| Block write failure (I/O) | generalProgrammingFailure | 0x72 |
| Received bytes ≠ announced size | conditionsNotCorrect | 0x22 |
| CRC mismatch | generalProgrammingFailure | 0x72 |

On any fatal error during transfer (I/O failure, CRC mismatch, size mismatch)
the staging file is deleted and the session is reset to kFailed.  The node
resumes normal lamp operations.  A new 0x34 can restart the OTA session.

---

## Production Path

To complete the OTA chain on real hardware:

1. **STM32 bare-metal:** The UDS OTA protocol is implemented in
   `OtaSessionManager`.  Wire incoming UDS frames to its handlers from an
   Ethernet-based UDS receiver (e.g., a LwIP TCP server on port 13400).
   Flash programming requires the STM32 Ethernet/UART bootloader or a
   custom flash-write routine behind the kValidatedPath staging file.

2. **Zephyr RTOS:** Add an `ota_thread` that receives UDS frames via a Zephyr
   UDP socket and delegates to `OtaSessionManager`.  Flash write via
   **MCUboot** (`boot_request_upgrade()`) after successful 0x37: MCUboot
   verifies the image signature on next reset and swaps it into the primary
   slot.

3. **Security:** Production OTA should add:
   - Authenticated extended session (0x27 SecurityAccess / challenge-response)
   - Signed firmware image verification (ECDSA or RSA)
   - A/B slot management to allow rollback on failed boot

---

## Demo Usage

```bash
# Start the rear lighting node simulator (in a separate terminal):
tools/run_simulator.sh

# Dry-run OTA with 2048 synthetic bytes:
python3 tools/ota_client/ota_client.py --host 127.0.0.1 --dry-run

# Transfer a real binary:
python3 tools/ota_client/ota_client.py --host 127.0.0.1 \
    --firmware /path/to/image.bin

# Verify the node returns kUpdating (0x04) during transfer:
python3 tools/uds_client/bcl_diagnostic_client.py \
    --host 127.0.0.1 --read-node-health
# Should show: state  UPDATING
```

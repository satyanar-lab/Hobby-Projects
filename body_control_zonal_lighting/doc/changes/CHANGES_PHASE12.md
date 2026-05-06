# Phase 12 — OTA Firmware Update (UDS 0x34/0x36/0x37 over DoIP)

## Summary

Phase 12 adds a credible OTA firmware update demonstration using ISO 14229-1
transfer services (RequestDownload / TransferData / RequestTransferExit) over
the existing DoIP TCP diagnostic channel.  The Linux simulator accepts a full
firmware transfer, streams blocks to a staging file, validates size and CRC-32,
and promotes the image on success.  STM32/Zephyr builds document the OTA
integration point.  A Python OTA client drives the complete sequence.

---

## 1 — Domain Layer

### Updated: `include/body_control/lighting/domain/uds_service_ids.hpp`

New service IDs:
- `kSidRequestDownload = 0x34`
- `kSidTransferData = 0x36`
- `kSidRequestTransferExit = 0x37`

New NRCs:
- `kNrcUploadDownloadNotAccepted = 0x70`
- `kNrcTransferDataSuspended = 0x71`
- `kNrcGeneralProgrammingFailure = 0x72`
- `kNrcWrongBlockSequenceCounter = 0x73`

New node health state constants for F102 DID encoding:
- `kHealthUnknown = 0x00`, `kHealthOperational = 0x01`, `kHealthDegraded = 0x02`
- `kHealthFaulted = 0x03`, `kHealthUpdating = 0x04`

---

## 2 — Application Layer

### New file: `include/body_control/lighting/application/ota_session_manager.hpp`
### New file: `src/application/ota_session_manager.cpp`

`OtaSessionManager` — dedicated state machine for UDS OTA transfer.

**State transitions:**
```
kIdle → kActive (on 0x34) → kComplete (on successful 0x37)
                           → kFailed   (on size/CRC mismatch or I/O error)
kComplete/kFailed → kActive (new 0x34 allowed to restart)
```

**Protocol details:**
- `HandleRequestDownload`: ALFID=0x44 required; parses 4-byte big-endian size;
  opens `/tmp/ota_firmware_staging.bin` with O_TRUNC; responds with
  `[0x74][0x20][0x02][0x02]` (maxBlockLength=514)
- `HandleTransferData`: validates blockSequenceCounter (wraps 0xFF→0x00);
  appends data to staging file; updates running CRC-32 incrementally
- `HandleRequestTransferExit`: validates received_size == expected_size;
  if 5-byte request, validates CRC-32 (ISO 3309, polynomial 0xEDB88320);
  fsync + rename to `/tmp/ota_firmware_validated.bin` on success

**CRC-32 algorithm:** CRC-32/ISO-HDLC (Ethernet FCS / ZIP). Computed
incrementally per block. Finalized as `running_crc ^ 0xFFFFFFFF`.

**F102 NodeHealth DID:** `EncodeNodeHealth` returns `kHealthUpdating (0x04)`
while `OtaSessionManager::IsOtaModeActive()` is true.

### Updated: `include/body_control/lighting/application/uds_request_handler.hpp`
### Updated: `src/application/uds_request_handler.cpp`

- `OtaSessionManager ota_session_manager_` added as value member
- Dispatch cases for 0x34, 0x36, 0x37 delegate to `ota_session_manager_`
- `EncodeNodeHealth`: `kHealthUpdating` takes priority over `kHealthFaulted`

---

## 3 — Python OTA Client

### New: `tools/ota_client/ota_client.py`
### New: `tools/ota_client/requirements.txt`

No external dependencies (uses Python `zlib` for CRC-32).

```
python3 ota_client.py --host 127.0.0.1 --dry-run
python3 ota_client.py --host 127.0.0.1 --dry-run --size 4096
python3 ota_client.py --host 127.0.0.1 --firmware /path/to/image.bin
python3 ota_client.py --host 127.0.0.1 --firmware /path/to/image.bin --quiet
```

**Sequence:**
1. Enter extended diagnostic session (0x10 0x03)
2. 0x34 RequestDownload with ALFID=0x44, size=len(firmware)
3. Split into 512-byte blocks; send each as 0x36 with incrementing blockSeq
4. Print progress: `Transferring:  45% (23/51 blocks)`
5. 0x37 RequestTransferExit with 4-byte CRC-32 (big-endian)

---

## 4 — STM32 / Zephyr Integration Points

### Modified: `app/stm32_nucleo_h753zi/main.cpp`
### Modified: `app/zephyr_nucleo_h753zi/main.cpp`

Both targets have an OTA integration-point comment showing how to wire
`OtaSessionManager` handlers to incoming UDS frames.  No actual flash write
is performed; that requires:
- **STM32 bare-metal:** STM32 Ethernet/UART bootloader or custom flash driver
- **Zephyr RTOS:** MCUboot (`boot_request_upgrade()`) with signed image

---

## 5 — Unit Tests

### New: `test/unit/test_ota_handler.cpp`

12 test cases covering:

| Test | Scenario |
|---|---|
| RequestDownload_ValidRequest_ReturnsMaxBlockLength | 0x74 response, 514 maxBlockLength |
| RequestDownload_ShortRequest_ReturnsNrc | malformed request |
| RequestDownload_ZeroSize_ReturnsNrc | zero size field |
| RequestDownload_WhileActive_ReturnsUploadDownloadNotAccepted | 0x34 re-entry |
| TransferData_OutsideSession_ReturnsNrc | 0x36 without prior 0x34 |
| TransferData_FirstBlock_AcceptedAndEchoed | blockSeq echo in response |
| TransferData_OutOfSequence_ReturnsNrc | kNrcWrongBlockSequenceCounter |
| IsOtaModeActive_DuringTransfer_ReturnsTrue | state query |
| RequestTransferExit_OutsideSession_ReturnsNrc | 0x37 without prior 0x34 |
| RequestTransferExit_SizeMismatch_ReturnsNrc | fewer bytes than declared |
| RequestTransferExit_CrcMismatch_ReturnsNrc | wrong CRC in 0x37 |
| FullTransfer_NoCrc_SuccessAndBecomesIdle | complete transfer, no CRC |
| FullTransfer_WithCrc_SuccessAndBecomesIdle | complete transfer with CRC |
| IsOtaModeActive_AfterComplete_ReturnsFalse | state after kComplete |
| MultipleBlocks_SequenceWrapsCorrectly | 3-block sequence |

---

## 6 — Documentation

### New: `doc/ota_specification.md`

- OTA sequence diagram
- UDS service mapping table
- Block size and CRC algorithm
- NodeHealthStatus state table (kUpdating)
- Error handling NRC table
- Production path (STM32 bootloader / MCUboot)
- Demo usage commands

### Updated: `tools/uds_client/bcl_diagnostic_client.py`

Added `4: 'UPDATING'` to `HEALTH_STATE` dict so `--read-node-health`
displays the correct label during OTA transfer.

---

## 7 — Build Changes

`src/CMakeLists.txt`:
- `application/ota_session_manager.cpp` added to `CORE_SOURCES_COMMON`

`test/unit/CMakeLists.txt`:
- `body_control_lighting_add_test(test_ota_handler.cpp)` added

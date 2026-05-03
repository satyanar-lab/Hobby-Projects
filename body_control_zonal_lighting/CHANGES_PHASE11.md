# Phase 11 — UDS Diagnostics over DoIP (ISO 14229-1 + ISO 13400-2)

## Summary

Phase 11 adds a standards-based diagnostic channel running in parallel with
the SOME/IP operational path.  The rear lighting node simulator listens on
TCP port 13400 (DoIP) and responds to UDS service requests.  A Python
diagnostic client provides a command-line tester interface.  Unit tests
cover all implemented UDS services.

---

## 1 — Domain Layer

### New file: `include/body_control/lighting/domain/uds_service_ids.hpp`

| Constant | Value | Purpose |
|---|---|---|
| `kSidDiagnosticSessionControl` | 0x10 | Session control SID |
| `kSidClearDiagnosticInformation` | 0x14 | Clear all DTCs |
| `kSidReadDtcInformation` | 0x19 | Read DTC info sub-functions |
| `kSidReadDataByIdentifier` | 0x22 | Read DID |
| `kSidRoutineControl` | 0x31 | Routine inject/clear fault |
| `kSidNegativeResponse` | 0x7F | NR SID |
| `kPositiveResponseOffset` | 0x40 | Added to SID for positive response |
| `kNrcServiceNotSupported` | 0x11 | NRC |
| `kNrcSubFunctionNotSupported` | 0x12 | NRC |
| `kNrcRequestOutOfRange` | 0x31 | NRC |
| `kDidEcuIdentification` | 0xF190 | ECU ID string |
| `kDidLampStatus` | 0xF101 | 5-byte lamp output state snapshot |
| `kDidNodeHealth` | 0xF102 | 3-byte health/flags/fault-count |
| `kDidActiveFaults` | 0xF103 | 1-byte count + N×2-byte DTC codes |
| `kDoipTesterAddress` | 0x0E00 | DoIP logical address — tester |
| `kDoipRearNodeAddress` | 0x0E01 | DoIP logical address — rear node |
| `kDoipPort` | 13400 | TCP port (ISO 13400-2) |
| `kDtcSubFuncReportByStatusMask` | 0x02 | 0x19 sub-function |
| `kDtcSubFuncReportSupportedDtcs` | 0x0A | 0x19 sub-function |
| `kSessionDefault` | 0x01 | Default diagnostic session |
| `kSessionExtended` | 0x03 | Extended diagnostic session |
| `kRoutineSubFuncStart` | 0x01 | RoutineControl start (inject) |
| `kRoutineSubFuncStop` | 0x02 | RoutineControl stop (clear) |

---

## 2 — Application Layer

### New file: `include/body_control/lighting/application/uds_request_handler.hpp`
### New file: `src/application/uds_request_handler.cpp`

`UdsRequestHandler` owns no state beyond a reference to
`RearLightingFunctionManager`.  All methods are `noexcept`.

**Implemented services:**

| Service | SID | Request | Response |
|---|---|---|---|
| DiagnosticSessionControl | 0x10 | SID + session | 0x50 + session |
| ClearDiagnosticInformation | 0x14 | SID + 0xFFFFFF | 0x54 |
| ReadDTCInformation | 0x19 | SID + 0x02 + mask | 0x59 + sub + records |
| ReadDTCInformation | 0x19 | SID + 0x0A | 0x59 + 0x0A + 5 records |
| ReadDataByIdentifier | 0x22 | SID + DID_hi + DID_lo | 0x62 + DID + payload |
| RoutineControl | 0x31 | SID + 0x01/02 + routine_id | 0x71 + echo |

**DIDs:**
- `0xF190` — ECU identification string (`BODY_CONTROL_LIGHTING_PROJECT_VERSION`)
- `0xF101` — 5 × `uint8_t` lamp output state (kUnknown=0, kOff=1, kOn=2)
- `0xF102` — health_state (1 byte) + flags (eth_link|svc_avail|fault_present) + active_fault_count
- `0xF103` — active_fault_count + N × 2-byte FaultCode values

**RoutineControl** maps routine IDs 0xB001–0xB005 back to `LampFunction`
for inject (sub-function 0x01) and clear (sub-function 0x02).

---

## 3 — Transport Layer

### New file: `include/body_control/lighting/transport/doip_server.hpp`
### New file: `src/transport/doip_server.cpp`

`DoipServer` runs a single background `std::thread` that accepts one TCP
connection at a time on the DoIP port (default 13400).

**Protocol implemented (ISO 13400-2:2019):**

| Frame type | Value | Direction |
|---|---|---|
| RoutingActivationRequest | 0x0005 | client → server |
| RoutingActivationResponse | 0x0006 | server → client |
| DiagnosticMessage | 0x8001 | both |
| DiagnosticMessageAck | 0x8002 | server → client |

- `SO_REUSEADDR` on server socket for fast restart
- `Stop()` calls `shutdown(SHUT_RDWR)` to unblock `accept()`
- Payload sanity cap: 65 536 bytes to prevent OOM from malformed frames
- One UDS request → one ACK frame + one diagnostic response frame

---

## 4 — Simulator Integration

### Modified: `app/rear_lighting_node_simulator/main.cpp`

```
UdsRequestHandler uds_handler {rear_lighting_function_manager};
DoipServer        doip_server  {uds_handler};
// ...
doip_server.Start();   // after SOME/IP init
// ...
doip_server.Stop();    // before SOME/IP shutdown
```

The DoIP server shares `rear_lighting_function_manager` with the SOME/IP
service provider.  Access is not mutex-guarded; concurrent access is an
acceptable limitation for a portfolio demo.

---

## 5 — Python Diagnostic Client

### New: `tools/uds_client/bcl_diagnostic_client.py`
### New: `tools/uds_client/requirements.txt`

Raw TCP DoIP client (no external dependencies).

```
python bcl_diagnostic_client.py --host 127.0.0.1 --read-ecu-info
python bcl_diagnostic_client.py --host 127.0.0.1 --read-lamp-status
python bcl_diagnostic_client.py --host 127.0.0.1 --read-node-health
python bcl_diagnostic_client.py --host 127.0.0.1 --read-fault-codes
python bcl_diagnostic_client.py --host 127.0.0.1 --read-dtc
python bcl_diagnostic_client.py --host 127.0.0.1 --clear-dtc
python bcl_diagnostic_client.py --host 127.0.0.1 --inject-fault 1
python bcl_diagnostic_client.py --host 127.0.0.1 --clear-fault 1
python bcl_diagnostic_client.py --host 127.0.0.1 --all
```

**DoIP logical addresses:** tester = 0x0E00, rear node = 0x0E01

---

## 6 — Unit Tests

### New: `test/unit/test_uds_request_handler.cpp`

14 test cases in `UdsHandlerTest` fixture:

| Test | Service | Scenario |
|---|---|---|
| ReadLampStatus_AllOff_ReturnsKOffForAllFunctions | 0x22/F101 | freshly constructed |
| ReadLampStatus_HeadLampOn_ReturnsKOnForHeadLamp | 0x22/F101 | after ActivateLamp |
| ReadActiveFaults_NoFaults_CountIsZero | 0x22/F103 | no faults |
| ReadActiveFaults_AfterInject_FaultCodePresent | 0x22/F103 | after InjectFault |
| ClearAllDtcs_ClearsFaultManager | 0x14 | 0xFFFFFF group |
| ClearDtcs_UnknownGroup_ReturnsNrc | 0x14 | non-0xFFFFFF group |
| RoutineControl_InjectFault_FaultManagerUpdated | 0x31 | sub-func 0x01 |
| RoutineControl_ClearFault_FaultManagerUpdated | 0x31 | sub-func 0x02 |
| RoutineControl_UnknownRoutineId_ReturnsNrc | 0x31 | unknown ID |
| UnknownSid_ReturnsServiceNotSupported | 0x7F | unknown SID |
| EmptyRequest_ReturnsServiceNotSupported | 0x7F | zero-length |
| DiagnosticSessionControl_DefaultSession | 0x10 | session 0x01 |
| DiagnosticSessionControl_ExtendedSession | 0x10 | session 0x03 |
| DiagnosticSessionControl_UnknownSession | 0x10 | unknown session |
| ReadDtcByStatusMask_NoFaults | 0x19/0x02 | empty list |
| ReadDtcByStatusMask_AfterInject | 0x19/0x02 | one record |
| ReadDtcSupported_ReturnsFiveEntries | 0x19/0x0A | 5 × 4-byte records |

---

## 7 — Build Changes

`src/CMakeLists.txt`:
- `application/uds_request_handler.cpp` added to `CORE_SOURCES_COMMON`
- `transport/doip_server.cpp` added to `CORE_SOURCES_LINUX`

`test/unit/CMakeLists.txt`:
- `body_control_lighting_add_test(test_uds_request_handler.cpp)` added

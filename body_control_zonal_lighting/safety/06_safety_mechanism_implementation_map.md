# Safety Mechanism Implementation Map

**ISO 26262-6 (software), ISO 26262-9 (safety-related systems)**

Direct mapping of each safety mechanism to the specific source file, class or
function, and configuration item where it is implemented. Where a mechanism is
a documented gap, the production implementation path is described.

---

## Implemented Mechanisms

| Mechanism | Safety Goal | ISO 26262 Ref | Implementation File | Class / Function | Notes |
|---|---|---|---|---|---|
| Hazard priority arbitration | SG-01 | Part 3, clause 8 (FSC) | `src/application/command_arbitrator.cpp` | `CommandArbitrator::IsHazardCommand()` | Hazard commands bypass normal indicator arbitration; indicator fanout carries same sequence counter so companion commands are suppressed |
| Indicator exclusivity (left/right mutual lock) | SG-02 | Part 3, clause 8 | `app/zephyr_nucleo_h753zi/main.cpp` | `HandleIndicator()` | Activating one indicator deactivates the opposite before asserting the new output |
| Lamp state persistence (no autonomous clear) | SG-04 | Part 3, clause 8 | `src/application/exterior_lighting_function_manager.cpp` | `ExteriorLightingFunctionManager::ApplyCommand()` | State only changes on explicit `kActivate` or `kDeactivate`; no timer or keep-alive path |
| NodeHealthMonitor heartbeat watchdog | SG-05 | Part 9, clause 6.4 | `src/application/node_health_monitor.cpp` | `NodeHealthMonitor::ProcessMainLoop()` | 2000 ms timeout (`kNodeHeartbeatTimeout`); transitions node to `kUnavailable` on expiry |
| Heartbeat publisher | SG-05 | Part 9 | `app/zephyr_nucleo_h753zi/main.cpp` | `HealthThread()` | Publishes `NodeHealthStatus` every 1000 ms; stops if thread crashes → triggers CZC timeout |
| Diagnostic Trouble Code (DTC) generation | SG-02 (partial) | ISO 14229-1 + Part 9 | `src/application/fault_manager.cpp` | `FaultManager::InjectFault()` | Raises DTC 0xB001–0xB005 per lamp; currently simulation-only (UDS RoutineControl); not autonomous |
| DTC storage and reporting | SG-02 | ISO 14229-1 | `src/application/uds_request_handler.cpp` | `HandleReadDtcInformation()` | UDS 0x19 returns active DTCs; UDS 0x14 clears them |
| MCUboot image signature validation | FM-004 | Part 6, clause 9 (memory) | MCUboot bootloader (Zephyr sysbuild) | `boot_image_validate()` | ECDSA-P256 with test key; production requires secure key provisioning |
| OTA dual-bank flash (rollback on bad image) | FM-009 | Part 6, clause 9 | MCUboot + `src/application/ota_session_manager_zephyr.cpp` | `boot_request_upgrade(BOOT_UPGRADE_TEST)` | New image boots in test mode; `boot_write_img_confirmed()` called after first successful health TX |
| Stack overflow detection (MPU guard) | FM-003 | Part 6, clause 9 (memory) | `app/zephyr_nucleo_h753zi/prj.conf` | `CONFIG_HW_STACK_PROTECTION=y` | Cortex-M7 MPU places no-access guard page below each thread stack |
| SOME/IP payload size validation | FM-008 | Part 6, clause 6 (defensive programming) | `app/zephyr_nucleo_h753zi/main.cpp` | `LampCommandDispatcher::OnTransportMessageReceived()` | Rejects payloads != 5 bytes before parsing; prevents OOB read |
| OTA CRC-32 on transfer exit | FM-007 | Part 6, clause 9 | `src/application/ota_session_manager_zephyr.cpp` | `HandleTransferExit()` | Optional CRC validated if client sends 4-byte CRC in 0x37 request |
| Block sequence counter validation | FM-007 | Part 6, clause 9 | `src/application/ota_session_manager.cpp` | `OtaSessionManager::HandleTransferData()` | Expected block number tracked; out-of-order blocks rejected |
| Brown-out reset (BOR) | FM-006 | Hardware — Part 5 | STM32H753ZI hardware | BOR hardware block | Resets MCU on Vdd drop; all GPIO outputs de-energise at reset |

---

## Gap Mechanisms (Not Yet Implemented)

| Mechanism | Safety Goal / FM | Recommended Implementation | Priority |
|---|---|---|---|
| Hardware watchdog (IWDG) | FM-010, SG-05 | `CONFIG_WATCHDOG=y` + `CONFIG_IWDG_STM32=y` in `prj.conf`; kick from `HealthThread` every 1000 ms | High |
| GPIO output current-sense verification | FM-002, SG-02 | Connect driver IC DIAG pin to ADC; autonomous `FaultManager::InjectFault()` on stuck-on detection | High |
| Network rate limiting / source filter | FM-005 | Reject UDP datagrams from non-CZC source; token-bucket limiter in `ZephyrUdpTransportAdapter::ReceiveBlocking()` | Medium |
| Non-volatile lamp state (post-reset restore) | SG-04 | Store last commanded state to flash before reset; re-issue on boot (requires CZC co-ordination) | Low |
| Command-to-output latency measurement | SG-03 | Timestamp command at RX and GPIO write; assert ≤ 50 ms in `HealthThread` self-test | Low |

---

## Traceability Summary

```
HE-001 → SG-01 → CommandArbitrator hazard priority
                → (gap) GPIO output feedback sensing

HE-002 → SG-02 → ExteriorLightingFunctionManager stateless design
HE-005 → SG-02 → HandleIndicator mutual exclusion
                → FaultManager DTC 0xB001/0xB002 (simulation only)
                → (gap) autonomous DTC on stuck-on

HE-003 → SG-03 → CmdThread synchronous dispatch (< 25 ms measured)
                → (gap) runtime latency assertion

HE-004 → SG-04 → ApplyCommand — no autonomous state clear path
                → (gap) post-reset state restore

HE-006 → SG-05 → NodeHealthMonitor 2 s timeout → kUnavailable
                → HealthThread 1 s publish
                → (gap) hardware IWDG on rear node
```

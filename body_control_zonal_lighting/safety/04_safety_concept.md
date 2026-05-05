# Functional Safety Concept (FSC)

**ISO 26262-3, clause 8**

The Functional Safety Concept allocates safety goals to safety mechanisms and
identifies which mechanisms are architectural properties of the design vs.
active detection or recovery logic.

Three mechanism classes are used:

| Class | Description |
|---|---|
| **Prevention** | Architectural property that makes the failure mode impossible or negligible without requiring detection |
| **Detection** | Mechanism that detects the fault and raises an observable indication (DTC, log, health state change), without automatic recovery |
| **Detection + Recovery** | Mechanism that detects the fault and transitions the system to a defined safe state |

---

## SG-01 — Hazard lamp availability (ASIL B)

**Mechanism:** Hazard priority arbitration in `CommandArbitrator`.

**Class:** Prevention (architectural).

**Implementation:**
`src/application/command_arbitrator.cpp::IsHazardCommand()` identifies hazard
commands. In `app/zephyr_nucleo_h753zi/main.cpp::HandleHazard()`, a hazard
activation command sets `g_indicator_registry.hazard_active = true` and
records any currently active indicator in `g_indicator_registry.active_indicator`
before overriding both indicator outputs. Subsequent indicator commands are
blocked at `HandleIndicator()` while `IsOn(LF::kHazardLamp)` is true. The
hazard GPIO output is written unconditionally on every 20 ms blink tick while
the hazard state is asserted.

**Gap:** There is no output verification (no current sensing on the GPIO line).
The software commands the output but cannot confirm the relay/driver has
responded. A stuck-low driver fault would satisfy the software state machine
while the lamp remains off — a production implementation would add a feedback
ADC channel.

**Latency measured:** Command-to-GPIO latency is < 50 ms (network RX →
`k_msgq_put` → `CmdThread` → `ApplyLampCommand` → `gpio_.WriteLampOutput`).
This is well within the 200 ms SG-01 budget.

---

## SG-02 — No spurious indicator activation (ASIL A)

**Mechanism:** Stateless lamp function manager + arbitration rule enforcement.

**Class:** Prevention (architectural) + Detection (DTC).

**Implementation:**
`ExteriorLightingFunctionManager` (`src/application/exterior_lighting_function_manager.cpp`)
holds lamp state only as a direct reflection of the last `ApplyCommand` call.
There is no background task, timer, or autonomous actor that writes lamp state.
Indicator outputs are driven only from `BlinkThread` and `CmdThread`, both of
which read state under `g_lamp_mgr_mutex`. The mutex prevents data races that
could produce a transient phantom output state.

The `FaultManager` (`src/application/fault_manager.cpp`) raises DTC `0xB001`
(left indicator) or `0xB002` (right indicator) when a fault is injected via UDS
RoutineControl. This is currently a simulation path (test fault injection), not
a closed-loop detection mechanism. In production, a stuck-on indicator would be
detected by comparing the commanded state against a feedback signal from the
driver IC.

**Gap:** No hardware-level output verification. DTC B001/B002 are injected
manually via UDS, not raised autonomously on detection.

---

## SG-03 — Indicator deactivation latency (ASIL A)

**Mechanism:** Synchronous command dispatch — no buffering after the message queue.

**Class:** Prevention (architectural).

**Implementation:**
The `CmdThread` in `app/zephyr_nucleo_h753zi/main.cpp` dequeues commands from
`g_lamp_cmd_queue` (depth 8) and immediately calls `ApplyLampCommand`. The
queue is a FIFO; deactivation commands are not held or coalesced. In
`HandleIndicator()`, a `kDeactivate` action calls `g_lamp_mgr.ApplyCommand`
and then `SendLampStatusEvent` within the same thread invocation. GPIO is
updated on the next 20 ms blink tick (≤ 20 ms additional latency).

Total worst-case: SOME/IP RX → msgq enqueue → CmdThread dequeue (≤ 1 ms
scheduling) → ApplyCommand → next blink tick (≤ 20 ms) → GPIO write = **< 25 ms**.
This is well within the 200 ms SG-03 requirement.

**Gap:** The 200 ms requirement is not verified by a runtime assertion or
watchdog. Production would add a timestamped command-to-output trace and a
periodic self-check.

---

## SG-04 — Headlamp persistence (ASIL B)

**Mechanism:** No autonomous state clear path in the function manager.

**Class:** Prevention (architectural).

**Implementation:**
`ExteriorLightingFunctionManager::ApplyCommand` writes the new lamp state into
`lamp_statuses_` only when called explicitly. There is no timer, keep-alive, or
network-dependent state expiry. `ZephyrUdpTransportAdapter` in
`src/platform/zephyr/zephyr_udp_transport.cpp` calls the transport message
handler on receive, but never writes to the function manager directly — the
message handler only enqueues a `LampCommand` to `g_lamp_cmd_queue`. A network
drop therefore results in no new commands, and lamp state persists.

**Limitation:** On ECU reset (watchdog, hard fault, deliberate reboot), the
function manager is zero-initialised. All lamp states return to `kOff`. This
is the unavoidable power-on safe state in a system without non-volatile lamp
state storage. For production, the safe state after reset should be communicated
to the CZC so it can re-issue commands.

---

## SG-05 — Silent ECU detection (ASIL A)

**Mechanism:** `NodeHealthMonitor` heartbeat watchdog at the CZC.

**Class:** Detection + Recovery (partial).

**Implementation:**
`src/application/node_health_monitor.cpp::ProcessMainLoop()` accumulates
elapsed time since the last `UpdateNodeHealthStatus()` call. When
`time_since_last_health_update_` exceeds `kNodeHeartbeatTimeout = 2000 ms`
(defined in `include/body_control/lighting/domain/lighting_constants.hpp`), the
health state is set to `NodeHealthState::kUnavailable` and both
`service_available` and `ethernet_link_available` are cleared. The CZC's
`IsNodeAvailable()` method returns false, and the HMI receives the updated
health snapshot on the next poll cycle.

The rear node publishes `NodeHealthStatus` every `kNodeHealthPublishPeriod =
1000 ms` from `HealthThread`. Two consecutive missed events (2 × 1000 ms = 2000
ms) trigger the timeout — exactly the `kNodeHeartbeatTimeout` constant.

**Recovery:** The CZC stops sending new commands to the unavailable node (the
`ExteriorLightingServiceConsumer` guards sends on service availability). This
is a partial safe state — lamps hold their last state on the node side. Full
recovery requires the rear node to reboot and re-register, then the CZC to
re-issue lamp state.

**Gap:** There is no hardware watchdog on the rear node itself. A firmware
crash that locks up all threads (including `HealthThread`) would stop the
heartbeat, be detected by the CZC within 2 s, but the rear node would remain in
its last GPIO state with no automatic recovery. Production requires a hardware
IWDG (independent watchdog) configured to reset the MCU if the main application
does not kick it within the watchdog window.

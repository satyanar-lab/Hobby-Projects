# Safety Goals

**ISO 26262-3, clause 7**

Safety goals are top-level safety requirements derived from the HARA. Each
safety goal is assigned the ASIL of the hazardous event(s) that motivate it.
Safety goals are formulated in functional terms — they describe what must hold
at the item boundary, not how it is achieved.

---

## SG-01 — Hazard lamp availability (ASIL B)

**Derived from:** HE-001

**Goal:** The exterior lighting node shall activate the hazard lamp output
within 200 ms of receiving a hazard activation command from the CZC, and shall
maintain the output in the activated state until a hazard deactivation command
is received or power is lost.

**Safe state:** At least the hazard relay driver output is energised and
visible to nearby vehicles; individual indicator sub-outputs may be in any
state.

**Rationale:** Failure to activate hazard lamps on a highway hard shoulder
removes the primary warning signal to approaching traffic. The 200 ms latency
budget accounts for the SOME/IP command transmission time, kernel scheduling
jitter, and GPIO driver latency — all of which are well under 50 ms in the
current implementation.

---

## SG-02 — No spurious indicator activation (ASIL A)

**Derived from:** HE-002, HE-005

**Goal:** Indicator and hazard outputs shall not be asserted unless a
corresponding activation command from the CZC is active in the command queue.
No persistent state error or memory fault shall cause an indicator to activate
or remain active after the commanding source has been cleared.

**Safe state:** All indicator outputs de-energised when no command is present
in the function manager state.

**Rationale:** A spurious indicator is directly observable by surrounding
traffic and can trigger evasive manoeuvres. The safe state is the de-energised
condition — incorrect activation is more hazardous than incorrect
de-activation for brief durations.

---

## SG-03 — Indicator deactivation latency (ASIL A)

**Derived from:** HE-003

**Goal:** An indicator deactivation command received at the network interface
shall result in the corresponding GPIO output being de-energised within 200 ms.

**Safe state:** Indicator output off after the deactivation command has been
received and processed by the function manager.

**Rationale:** A stuck indicator persisting beyond 200 ms after driver intent
is a misleading signal to other road users. The 200 ms budget is deliberately
conservative; the implementation processes commands in under 50 ms
(SOME/IP RX → `k_msgq_put` → `CmdThread` → `ApplyCommand` → GPIO write).

---

## SG-04 — Headlamp persistence (ASIL B)

**Derived from:** HE-004

**Goal:** The headlamp output shall remain in its last commanded state
indefinitely in the absence of a deactivation command. A network interruption,
software exception, or watchdog reset shall not spontaneously de-energise
the headlamp output.

**Safe state:** For a spontaneous off failure: driver must notice and react
(illumination by cockpit warning). For a spontaneous on failure: lamp persists
(less hazardous than dark road). The target safe state is "remain on" during a
fault until an explicit off command arrives.

**Rationale:** Lamp state in the function manager
(`ExteriorLightingFunctionManager`) is only modified by explicit `ApplyCommand`
calls; no background task clears state. A network drop does not call
`ApplyCommand`, so state persists. A firmware reset returns state to the
zero-initialised (all off) safe state — which is not ideal for SG-04 but is
the only feasible power-on state without non-volatile storage.

---

## SG-05 — Silent ECU detection (ASIL A)

**Derived from:** HE-006

**Goal:** The Central Zone Controller shall detect a complete loss of
responsiveness from the rear lighting node within 2 seconds of the last
received NodeHealthStatus event, and shall report the rear node state as
`kUnavailable` to any connected diagnostic or HMI client.

**Safe state:** CZC marks rear node unavailable; HMI displays fault indicator;
no new lamp commands are sent to the node (they would be silently dropped).

**Rationale:** The `NodeHealthMonitor` in `src/application/node_health_monitor.cpp`
implements a watchdog timer using `kNodeHeartbeatTimeout = 2000 ms`
(`include/body_control/lighting/domain/lighting_constants.hpp:55`). The rear
node publishes a heartbeat every `kNodeHealthPublishPeriod = 1000 ms`. Two
missed heartbeats trigger `kUnavailable`. The 2-second timeout value directly
satisfies this safety goal.

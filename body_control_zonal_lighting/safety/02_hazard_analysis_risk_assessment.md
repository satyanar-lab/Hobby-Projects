# Hazard Analysis and Risk Assessment (HARA)

**ISO 26262-3, clause 6**

---

## 1. ASIL Classification Method

ASIL is determined by combining three parameters per ISO 26262 Table 4:

| Parameter | Scale | Meaning |
|---|---|---|
| Severity (S) | S0–S3 | Injury potential if the hazard leads to an accident |
| Exposure (E) | E0–E4 | Probability of the operational situation occurring |
| Controllability (C) | C0–C3 | Probability that the driver / other road users cannot avoid the hazard |

ASIL result ranges from QM (no safety requirement) through A, B, C, D.
Body-control lighting functions are typically ASIL A or B. ASIL C/D targets
are characteristic of brake-by-wire, steer-by-wire, and propulsion control —
not lighting actuator nodes.

---

## 2. Hazardous Events

### HE-001 — Hazard lamps fail to activate when commanded

**Operational situation:** Vehicle stationary on highway hard shoulder after
breakdown. Driver activates hazard switch.

**Hazard:** Nearby traffic cannot see the stopped vehicle; rear-end collision
risk.

| S | E | C | ASIL |
|---|---|---|---|
| S2 | E3 | C2 | **B** |

**Rationale:** Injury potential is moderate to severe (S2) — a rear-end
collision at highway speed causes significant harm, though the vehicle is
stationary and the driver has some warning time to seek shelter. Exposure is
occasional (E3) — breakdowns requiring hazard activation occur several times
per year for an average driver. Controllability is limited (C2): approaching
vehicles at speed on a dark hard shoulder have reduced ability to react,
especially in poor weather.

---

### HE-002 — Indicator activates without driver command

**Operational situation:** Lane change manoeuvre at highway speed.

**Hazard:** Following vehicle misinterprets the spurious indicator as an
intended lane change; adjusts position; near-miss or collision.

| S | E | C | ASIL |
|---|---|---|---|
| S1 | E4 | C2 | **A** |

**Rationale:** Light injuries are plausible (S1) — a false indicator may cause
a following vehicle to brake or swerve unnecessarily. Exposure is high (E4) —
lane changes are performed continuously on any highway journey. Controllability
is limited (C2): the driver cannot suppress the spurious indicator directly from
the vehicle interior, and following drivers have limited reaction time.

---

### HE-003 — Indicator fails to deactivate after lane change

**Operational situation:** Highway driving post lane-change; driver cancels
indicator but output stays on.

**Hazard:** Other road users are misled about the driver's continued intention
to change lanes, creating a misleading traffic situation.

| S | E | C | ASIL |
|---|---|---|---|
| S1 | E4 | C3 | **A** |

**Rationale:** Severity is light injury (S1) — a stuck indicator is more of a
nuisance than an immediate crash cause, but can trigger unexpected manoeuvres
from following vehicles. Exposure is high (E4). Controllability is more
difficult than HE-002 (C3) because the driver typically has no cockpit feedback
indicating the indicator is still on; the vehicle interior does not always have
an audible or visible indicator-on warning for the rear lights.

---

### HE-004 — Headlamp deactivates spontaneously at night

**Operational situation:** Night driving on an unlit rural road.

**Hazard:** Driver loses forward illumination; obstacle on road is not seen in
time to brake.

| S | E | C | ASIL |
|---|---|---|---|
| S2 | E3 | C2 | **B** |

**Rationale:** Severity is moderate (S2) — sudden loss of headlamps at speed
on an unlit road is likely to cause a significant crash. Exposure is occasional
(E3) — night driving on unlit roads is a regular but not constant occurrence.
Controllability is limited (C2): the driver may not immediately recognise the
lamp has failed and cannot react before reaching an obstacle.

---

### HE-005 — Both indicators activate simultaneously without driver command (false hazard)

**Operational situation:** Normal driving in traffic.

**Hazard:** Other vehicles interpret both indicators flashing as a hazard
declaration; some vehicles may stop or move aside, disrupting traffic flow.

| S | E | C | ASIL |
|---|---|---|---|
| S1 | E4 | C2 | **A** |

**Rationale:** A false hazard indication is visible to multiple surrounding
vehicles simultaneously (S1 — light injuries from traffic disruption; a full
emergency stop triggered by a false hazard in heavy motorway traffic could cause
chain collisions). Exposure is continuous when driving (E4). Controllability is
limited (C2): the driver has no immediate means to suppress the false hazard
output from the cockpit without stopping the vehicle.

---

### HE-006 — All lamps fail (silent ECU)

**Operational situation:** Any driving situation, typically detected only when
lamps are needed.

**Hazard:** No lighting function is available; all five outputs are off
regardless of driver command.

| S | E | C | ASIL |
|---|---|---|---|
| S1 | E4 | C2 | **A** |

**Rationale:** Severity is light injury (S1) — complete lamp failure removes
all signalling capability; the risk materialises when a lighting function is
actively needed (lane change, emergency stop, night driving). Exposure is high
(E4) — a silent ECU after a firmware crash could persist across an entire
journey undetected. Controllability is limited (C2): neither driver nor other
road users have immediate feedback that all lamp functions are unavailable.

---

## 3. ASIL Summary Table

| HE-ID | Hazardous Event | S | E | C | ASIL |
|---|---|---|---|---|---|
| HE-001 | Hazard lamps fail to activate when commanded | S2 | E3 | C2 | **B** |
| HE-002 | Indicator activates without driver command | S1 | E4 | C2 | **A** |
| HE-003 | Indicator fails to deactivate after lane change | S1 | E4 | C3 | **A** |
| HE-004 | Headlamp deactivates spontaneously at night | S2 | E3 | C2 | **B** |
| HE-005 | Both indicators activate simultaneously (false hazard) | S1 | E4 | C2 | **A** |
| HE-006 | All lamps fail — silent ECU | S1 | E4 | C2 | **A** |

Maximum ASIL for this item: **ASIL B**. This is expected and consistent with
body-control lighting nodes across the automotive industry.

---

## 4. Scope Note

This HARA covers the rear lighting node in isolation. A complete vehicle-level
HARA would additionally consider:

- Interactions with the Central Zone Controller (CZC) — commands that arrive
  out of order, duplicate commands, or command storms
- Interactions with the vehicle bus architecture (CAN gateway not present in
  this design)
- Environmental hazards (EMC-induced bit flip, temperature-induced latch-up)
- Shared-failure modes if the CZC and rear node share a power domain

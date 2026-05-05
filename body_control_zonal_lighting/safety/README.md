# Functional Safety Documentation

## Scope and Disclaimer

This folder contains portfolio material demonstrating ISO 26262 functional
safety engineering applied to the body-control lighting feature.

**This is not a real safety case and does not claim ISO 26262 compliance.**
A production safety case requires a full Item definition, HARA, FSC, TSC,
hardware DFA, software unit testing per ASIL target, formal verification,
and an independent safety assessment. What follows is a representative sample
showing the engineering discipline and structured thinking the standard demands.

Where documents reference ISO 26262 clause numbers, the applicable edition is
ISO 26262:2018 (second edition).

---

## Context

The item is an **exterior lighting actuator node** implemented on an STM32
NUCLEO-H753ZI running Zephyr RTOS. The node receives lamp commands from a
Central Zone Controller over SOME/IP UDP, applies arbitration logic, and drives
five GPIO outputs (left indicator, right indicator, hazard, park lamp, headlamp).

The highest ASIL determined in the HARA for this feature is **ASIL B**,
consistent with body-control lighting functions. ASIL C/D is typical of
brake-by-wire, steer-by-wire, and propulsion control — not lighting.

---

## Document Map

| File | ISO 26262 Reference | Content |
|---|---|---|
| [01_item_definition.md](01_item_definition.md) | Part 3, clause 5 | Item boundary, functions, operating modes, environment |
| [02_hazard_analysis_risk_assessment.md](02_hazard_analysis_risk_assessment.md) | Part 3, clause 6 | HARA — 6 hazardous events with S/E/C/ASIL classification |
| [03_safety_goals.md](03_safety_goals.md) | Part 3, clause 7 | Safety goals derived from HARA, one per hazardous event group |
| [04_safety_concept.md](04_safety_concept.md) | Part 3, clause 8 | Functional Safety Concept — mechanism-to-code mapping |
| [05_fmea.md](05_fmea.md) | ISO 26262-9 / AIAG FMEA | Component-level FMEA with RPN scoring and gap analysis |
| [06_safety_mechanism_implementation_map.md](06_safety_mechanism_implementation_map.md) | ISO 26262-6, -9 | Direct mapping of safety mechanisms to source files |

---

## Implemented Safety Mechanisms (Summary)

| Mechanism | Status | Source |
|---|---|---|
| DTC generation (B001–B005) | Implemented | `src/application/fault_manager.cpp` |
| Heartbeat supervision (2 s timeout) | Implemented | `src/application/node_health_monitor.cpp` |
| MCUboot image integrity (ECDSA-P256) | Implemented | MCUboot + Zephyr sysbuild |
| Stack overflow detection | Implemented | `CONFIG_HW_STACK_PROTECTION=y` |
| Hazard priority arbitration | Implemented | `src/application/command_arbitrator.cpp` |
| Hardware watchdog timer | **Gap** | Not implemented |
| GPIO output current-sense verification | **Gap** | Not implemented |
| Network rate limiting | **Gap** | Not implemented |

---

## Related Documentation

- [doc/security_architecture.md](../doc/security_architecture.md) — overlapping
  concern: MCUboot signing, OTA integrity, UDS session control
- [doc/some_ip_service_discovery.md](../doc/some_ip_service_discovery.md) — SD
  wire protocol used for service availability monitoring

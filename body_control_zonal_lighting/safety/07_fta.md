# Fault Tree Analysis (FTA)

**ISO 26262-9, clause 8 — Analysis of dependent failures and safety analyses
(deductive techniques: fault tree, Annex B).**

Fault Tree Analysis is the deductive, top-down counterpart to the inductive,
bottom-up FMEA in [05_fmea.md](05_fmea.md). For each safety goal in
[03_safety_goals.md](03_safety_goals.md), the negation of the goal is taken
as the top event and decomposed through Boolean gates (`OR`, `AND`) into
basic events. Basic events are then cross-referenced to FMEA failure-mode
IDs so the two analyses corroborate one another.

---

## 1. Scope and Method

### Top events

One fault tree is constructed per safety goal. The top event is the
violation of that goal at the item boundary.

| Tree | Top Event | Safety Goal | ASIL |
|---|---|---|---|
| FT-01 | Hazard lamp output absent when commanded, or lost while still commanded | SG-01 | B |
| FT-02 | Indicator output asserted with no commanding source active | SG-02 | A |
| FT-03 | Indicator deactivation latency exceeds 200 ms | SG-03 | A |
| FT-04 | Headlamp output de-energises while last command is still `kActivate` | SG-04 | B |
| FT-05 | CZC fails to mark the rear node `kUnavailable` within 2 s of last heartbeat | SG-05 | A |

### Notation

ASCII tree form, gate annotated at each junction:

```
Top event
└─ [OR]                  ← any child suffices
   ├─ Intermediate A
   │  └─ [AND]           ← all children required
   │     ├─ Basic B-x
   │     └─ Basic B-y
   └─ Basic B-z          ← leaf
```

`B-n` are basic events (leaves). Where a leaf maps to an FMEA failure mode
the `FM-xxx` ID is given; otherwise it is an undeveloped event noted as
`(undeveloped)`.

### Quantification scope

This FTA is **qualitative**. No basic-event probabilities are assigned and
no top-event PMHF is computed. ISO 26262-5 ASIL B PMHF targets
(< 10⁻⁷ h⁻¹ single-point, < 10⁻⁸ h⁻¹ latent) would require component
failure-rate data (e.g., Siemens SN29500, IEC TR 62380) that is out of
scope for this portfolio. The minimal cut-set tabulation below is sufficient
to expose single-point-of-failure (SPOF) candidates without quantification.

---

## 2. FT-01 — Hazard lamp output absent when commanded (SG-01, ASIL B)

```
SG-01 violated: hazard lamp NOT energised within 200 ms of command,
                OR de-energises before deactivation command
└─ [OR]
   ├─ Activation failure (lamp never energises within 200 ms)
   │  └─ [OR]
   │     ├─ Command never reaches the function manager
   │     │  └─ [OR]
   │     │     ├─ B-01  Ethernet PHY down                     (FM-001)
   │     │     ├─ B-02  UDP datagram dropped — network flood  (FM-005, gap)
   │     │     ├─ B-03  SOME/IP parser rejects valid msg      (defensive overshoot)
   │     │     └─ B-04  k_msgq_put returns -ENOMSG (queue full) (FM-005 consequence)
   │     ├─ Command reaches manager but GPIO not written
   │     │  └─ [OR]
   │     │     ├─ B-05  CmdThread blocked > 200 ms — mutex hold (undeveloped)
   │     │     ├─ B-06  CmdThread blocked > 200 ms — prio inversion (undeveloped)
   │     │     └─ B-07  CommandArbitrator silently drops hazard   (defect)
   │     └─ GPIO written but lamp not illuminated
   │        └─ [OR]
   │           ├─ B-08  Driver IC stuck low                   (FM-002 variant, gap)
   │           ├─ B-09  Open lamp filament / relay coil       (FM-002 family)
   │           └─ B-10  Loss of lamp supply (battery / fuse)  (undeveloped)
   └─ Persistence loss (lamp de-energises while still commanded)
      └─ [OR]
         ├─ ECU reset returns to all-off safe state
         │  └─ [OR]
         │     ├─ B-11  Stack overflow → MemManage → reset    (FM-003)
         │     ├─ B-12  Brown-out reset                       (FM-006)
         │     ├─ B-13  Hardware watchdog reset (if IWDG added) (FM-010 mitigation)
         │     └─ B-14  Hard fault from runtime exception     (undeveloped)
         ├─ B-15  Driver IC latches low after activation       (FM-002 variant, gap)
         └─ B-16  Function manager clears state autonomously   (architecturally impossible — FSC SG-01)
```

### Minimal cut sets — FT-01

| # | Cut Set | Order | FMEA Link | SPOF? |
|---|---|---|---|---|
| 1 | {B-01} | 1 | FM-001 | **Yes** |
| 2 | {B-02} | 1 | FM-005 | **Yes** |
| 3 | {B-04} | 1 | FM-005 | **Yes** |
| 4 | {B-08} | 1 | FM-002 | **Yes — highest concern** |
| 5 | {B-09} | 1 | FM-002 | **Yes** |
| 6 | {B-10} | 1 | — | **Yes** (hardware out of node scope) |
| 7 | {B-11} | 1 | FM-003 | **Yes** (current safe state is "all off") |
| 8 | {B-12} | 1 | FM-006 | **Yes** |
| 9 | {B-15} | 1 | FM-002 | **Yes** |

### FT-01 observations

- Nine single-point-of-failure cut sets violate SG-01. The dominant family
  is GPIO output integrity (B-08/B-09/B-15 → FM-002) — the FMEA top RPN
  finding. An output current-sense feedback channel would convert these
  single-event cut sets into two-event cut sets (driver fault AND
  diagnostic-pin fault) and would drop the SPOF count substantially.
- ECU reset paths (B-11, B-12, B-13) all violate SG-01 today because there
  is no non-volatile lamp-state storage. The FSC notes this as the
  unavoidable power-on safe state; if SG-01 quantification later showed
  this dominates the top-event probability, NV-storage of the hazard bit
  would be the targeted mitigation.
- B-13 is interesting: adding the IWDG watchdog (FM-010 mitigation) would
  *introduce* a new SG-01 cut set if not paired with NV-state restore.
  This is a documented trade-off — FM-010 reduces FM-003 / firmware-hang
  exposure but at the cost of an additional reset-induced de-energisation
  path. Pair the two mitigations.

---

## 3. FT-02 — Spurious indicator assertion (SG-02, ASIL A)

```
SG-02 violated: indicator output asserted with no matching active
                command in the function manager
└─ [OR]
   ├─ B-17  Driver IC stuck high                              (FM-002, gap)
   ├─ B-18  GPIO bus glitch / EMC-induced toggle              (undeveloped — Part 5 EMC)
   ├─ B-19  Memory corruption flips lamp_statuses_ bit        (no ECC on STM32H7 SRAM, undeveloped)
   ├─ Software writes output without commanding source
   │  └─ [AND]
   │     ├─ B-20  Crafted SOME/IP frame parsed as activation  (FM-008 — mitigated by size check)
   │     └─ B-21  Size-check bypass (parser defect)           (defect — undeveloped)
   ├─ B-22  HandleIndicator() asserts both indicators on hazard exit (defect — see CHANGES_PHASE9)
   └─ B-23  BlinkThread phase carries indicator on while state == off (defect)
```

### Minimal cut sets — FT-02

| # | Cut Set | Order | FMEA Link | SPOF? |
|---|---|---|---|---|
| 1 | {B-17} | 1 | FM-002 | **Yes** |
| 2 | {B-18} | 1 | — | Yes (EMC-class, hardware design) |
| 3 | {B-19} | 1 | — | Yes (uncovered — no ECC) |
| 4 | {B-20, B-21} | 2 | FM-008 | No (size check is the barrier) |
| 5 | {B-22} | 1 | — | Yes (defect class — covered by unit test `test_command_arbitrator`) |
| 6 | {B-23} | 1 | — | Yes (defect class — covered by mutex + unit tests) |

### FT-02 observations

- FM-008 is the only multi-event cut set in this tree. The size-check
  defensive code in `LampCommandDispatcher::OnTransportMessageReceived()`
  acts as a barrier event in the FTA sense — both the malformed-payload
  acceptance AND the parser defect must coincide.
- B-19 (SRAM bit-flip) is uncovered. Production designs use ECC SRAM or
  duplicate-and-compare state. This is an honest gap; the Cortex-M7
  internal SRAM has no ECC and Zephyr does not periodically scrub
  `lamp_statuses_`. For ASIL A this is typically acceptable given the
  short retention time of the state, but it is the dominant remaining
  cut set after FM-002 is closed.

---

## 4. FT-03 — Indicator deactivation latency > 200 ms (SG-03, ASIL A)

```
SG-03 violated: deactivation command-to-output latency > 200 ms
└─ [OR]
   ├─ Command not enqueued in time
   │  └─ [OR]
   │     ├─ B-24  UDP datagram lost; retransmit > 200 ms      (FM-005 consequence)
   │     └─ B-25  k_msgq_put fails — queue full                (FM-005)
   ├─ Command enqueued but not dispatched in time
   │  └─ [OR]
   │     ├─ B-26  CmdThread starved by higher-prio loop       (undeveloped — design defect)
   │     ├─ B-27  g_lamp_mgr_mutex held > 200 ms by HealthThread (undeveloped — design defect)
   │     └─ B-28  Priority inversion (no mutex prio inheritance) (Zephyr default — CONFIG_PRIORITY_CEILING)
   ├─ B-29  Blink-tick period extended > 200 ms by scheduler pressure  (undeveloped)
   └─ B-30  GPIO write succeeds but output stuck high          (FM-002 variant, gap)
```

### Minimal cut sets — FT-03

| # | Cut Set | Order | FMEA Link | SPOF? |
|---|---|---|---|---|
| 1 | {B-24} | 1 | FM-005 | Yes (network) |
| 2 | {B-25} | 1 | FM-005 | Yes |
| 3 | {B-26} | 1 | — | Yes (design) |
| 4 | {B-27} | 1 | — | Yes (design) |
| 5 | {B-28} | 1 | — | Yes (config) |
| 6 | {B-30} | 1 | FM-002 | **Yes** |

### FT-03 observations

- All cut sets are order-1 because the chain is purely sequential: any
  link breaks the latency budget. The measured worst case in
  [04_safety_concept.md](04_safety_concept.md) is < 25 ms; the 200 ms
  budget gives ~8× margin against scheduling jitter. The exposed risk is
  not nominal-path latency but **degenerate** cases (B-26/B-27/B-28).
- Recommendation from the safety mechanism map ("command-to-output
  latency measurement, priority Low") should be re-tagged **Medium** —
  it covers four of the six cut sets here as a detection mechanism, even
  without redesign.

---

## 5. FT-04 — Headlamp spontaneous de-energisation (SG-04, ASIL B)

```
SG-04 violated: headlamp output de-energises while last command was kActivate
└─ [OR]
   ├─ ECU reset clears volatile lamp state
   │  └─ [AND]
   │     ├─ B-31  Any reset cause occurs
   │     │  └─ [OR]
   │     │     ├─ B-31a  Stack overflow → reset       (FM-003)
   │     │     ├─ B-31b  Brown-out reset              (FM-006)
   │     │     ├─ B-31c  Hardware watchdog reset      (FM-010 mitigation introduces)
   │     │     ├─ B-31d  Hard fault                   (undeveloped)
   │     │     └─ B-31e  OTA reset for new image      (intentional — MCUboot swap)
   │     └─ B-32  No NV-state restore mechanism       (FSC gap — by design)
   ├─ B-33  Driver IC stuck low                                (FM-002 variant, gap)
   ├─ B-34  Memory corruption clears lamp_statuses_[headlamp]  (no ECC, undeveloped)
   ├─ B-35  Erroneous ApplyCommand(headlamp, kDeactivate) — defect (undeveloped)
   └─ B-36  Function manager autonomous clear                  (architecturally impossible — FSC SG-04)
```

### Minimal cut sets — FT-04

| # | Cut Set | Order | FMEA Link | SPOF? |
|---|---|---|---|---|
| 1 | {B-31a, B-32} | 2 | FM-003 + FSC gap | No (paired) |
| 2 | {B-31b, B-32} | 2 | FM-006 + FSC gap | No (paired) |
| 3 | {B-31c, B-32} | 2 | FM-010 mitigation + FSC gap | No (paired) |
| 4 | {B-31d, B-32} | 2 | — + FSC gap | No (paired) |
| 5 | {B-31e, B-32} | 2 | — + FSC gap | No (paired) — accepted (rare, planned) |
| 6 | {B-33} | 1 | FM-002 | **Yes** |
| 7 | {B-34} | 1 | — | Yes (no ECC) |
| 8 | {B-35} | 1 | — | Yes (defect class) |

### FT-04 observations

- The reset-induced de-energisation paths are all order-2 because each
  requires a reset cause **and** the absence of NV-state restore (B-32).
  Closing B-32 by adding NV storage of last headlamp state collapses
  five cut sets simultaneously — this is the highest-leverage SG-04
  mitigation.
- B-33 (FM-002 stuck-low variant) remains a SPOF for SG-04 even after
  NV-state restore. Output sensing addresses both SG-01 and SG-04 stuck-low
  failures.

---

## 6. FT-05 — CZC fails to detect silent rear node (SG-05, ASIL A)

```
SG-05 violated: CZC fails to set rear node state to kUnavailable within
                2 s of the last received NodeHealthStatus
└─ [OR]
   ├─ NodeHealthMonitor watchdog never triggers
   │  └─ [OR]
   │     ├─ B-37  kNodeHeartbeatTimeout misconfigured > 2000 ms (constant change — defect)
   │     ├─ B-38  CZC main loop blocked > 2 s                   (undeveloped — design defect)
   │     └─ B-39  ProcessMainLoop() never invoked               (CZC scheduler defect)
   ├─ Heartbeats appear to arrive though rear node is silent
   │  └─ [OR]
   │     ├─ B-40  Replayed heartbeat from attacker              (security — out of scope here, see security_architecture.md)
   │     └─ B-41  Stale heartbeat buffered in transport layer   (FSC: heartbeats not buffered — undeveloped)
   └─ Detection occurs but availability state not propagated
      └─ [OR]
         ├─ B-42  IsNodeAvailable() returns stale cached value  (defect — covered by unit test)
         └─ B-43  HMI poll delay > 2 s after detection           (HMI poll period 80 ms — not credible)
```

### Minimal cut sets — FT-05

| # | Cut Set | Order | FMEA Link | SPOF? |
|---|---|---|---|---|
| 1 | {B-37} | 1 | — | Yes (config defect — guard by unit test) |
| 2 | {B-38} | 1 | — | Yes (design defect) |
| 3 | {B-39} | 1 | — | Yes (scheduler defect) |
| 4 | {B-40} | 1 | — | Out of FSC scope (security) |
| 5 | {B-42} | 1 | — | Yes (defect) |

### FT-05 observations

- The dominant SG-05 risks are on the **CZC side**, not the rear node
  side. This is the correct architectural conclusion — SG-05 is a
  detection goal at the CZC. The rear node contributes only by being
  silent, which is the input to detection, not a failure of detection.
- B-40 (replayed heartbeat) is the intersection between functional
  safety and security. It is documented but allocated to the security
  case ([doc/security_architecture.md](../doc/security_architecture.md))
  rather than handled here. The SOME/IP layer offers no replay
  protection; production would add MACsec or session counters in the
  NodeHealthStatus payload.

---

## 7. Single-Point-of-Failure Roll-up

Order-1 cut sets aggregated across all trees and grouped by FMEA cause.
This is the FTA's central output: the minimal events that, alone, violate
any safety goal.

| Cause | FMEA ID | Goals Affected | Cut Sets |
|---|---|---|---|
| Driver IC stuck high / stuck low / open | FM-002 | SG-01, SG-02, SG-03, SG-04 | B-08, B-09, B-15, B-17, B-30, B-33 |
| Network flood / source-IP spoofing | FM-005 | SG-01, SG-03 | B-02, B-04, B-24, B-25 |
| Ethernet PHY hardware failure | FM-001 | SG-01 | B-01 |
| Stack overflow → reset (no NV restore) | FM-003 | SG-01 (direct), SG-04 (paired) | B-11 (FT-01); B-31a (FT-04 paired) |
| Brown-out reset (no NV restore) | FM-006 | SG-01 (direct), SG-04 (paired) | B-12 (FT-01); B-31b (FT-04 paired) |
| Hazard relay supply loss | hardware | SG-01 | B-10 |
| SRAM bit-flip on lamp_statuses_ | — (no ECC) | SG-02, SG-04 | B-19, B-34 |
| Heartbeat-timeout misconfiguration | — | SG-05 | B-37 |
| Design defects (priority inversion, mutex hold) | — | SG-03 | B-26, B-27, B-28 |

**FM-002 is the dominant single-point-of-failure family**, contributing
six order-1 cut sets across four of the five safety goals. This corroborates
the FMEA's RPN-120 ranking (FM-002 highest). Adding GPIO output current-sense
feedback per the FMEA recommendation converts all six cut sets to order-2,
which is the single highest-leverage safety mechanism the node currently lacks.

**FM-005 (network flood) is the second-most-impactful** with four order-1
cut sets across SG-01 and SG-03. Source-IP filtering + token-bucket rate
limiting converts these to order-2.

**The reset-induced cut sets are split**: in FT-01 they are order-1
because SG-01 requires the hazard lamp to remain on through the reset;
in FT-04 they are order-2 because each requires a reset **and** the
absence of NV-state restore. Adding NV-state restore closes FT-04 paths
but does not help FT-01 unless the NV write is fast enough to capture
the hazard-on transition before brown-out completes (BOR threshold).

---

## 8. Recommendations Derived From This FTA

Ranked by number of cut sets closed:

| # | Mitigation | Cut Sets Closed | Status in safety mechanism map |
|---|---|---|---|
| 1 | GPIO output current-sense feedback (close FM-002) | 6 order-1 → order-2 | Gap, High priority |
| 2 | Network source-IP filter + rate limiter (close FM-005) | 4 order-1 → order-2 | Gap, Medium priority |
| 3 | NV-storage of last commanded lamp state (close B-32) | 5 cut sets in FT-04 (paired form) | Gap, currently Low priority — **re-rank Medium** based on this FTA |
| 4 | Command-to-output latency runtime assertion | 4 cut sets in FT-03 (detection only) | Gap, currently Low priority — **re-rank Medium** |
| 5 | Mutex priority inheritance (`CONFIG_POLL` review) | B-28 | Not currently a documented mechanism |
| 6 | ECC SRAM or duplicate-and-compare lamp_statuses_ | B-19, B-34 | Not documented — accepted gap at ASIL A/B |
| 7 | Heartbeat replay protection (security overlap) | B-40 | Documented in security architecture |

The two re-ranks (items 3 and 4) are the principal new findings from
the FTA — they would not be visible from the FMEA alone because FMEA
treats each failure mode in isolation, while the FTA exposes how
multiple failure modes combine against a single safety goal.

---

## 9. Traceability — FTA to FMEA to FSC

```
SG-01 ──► FT-01 ──► 9 order-1 cut sets ──► FM-001, FM-002, FM-003,
                                            FM-005, FM-006
SG-02 ──► FT-02 ──► 4 order-1 + 1 order-2 ──► FM-002, FM-008 (mitigated)
SG-03 ──► FT-03 ──► 6 order-1 cut sets ──► FM-002, FM-005
SG-04 ──► FT-04 ──► 1 order-1 + 5 order-2 paired ──► FM-002, FM-003, FM-006
SG-05 ──► FT-05 ──► 5 order-1 cut sets ──► (no FMEA mapping —
                                            CZC-side software defects)
```

The FTA confirms the FMEA's prioritisation (FM-002 first, FM-010
second per RPN; FM-005 third) and adds two findings the FMEA does not
surface independently:

1. SG-05's principal cut sets are on the CZC side, not the rear node.
   FMEA scope was the node only; FTA forced consideration of the
   complete detection chain.
2. NV-state restore is more important than the safety-mechanism map
   currently ranks it, because it closes five FT-04 cut sets that span
   multiple distinct reset causes.

---

## 10. Limitations

- **Qualitative only.** No basic-event probabilities, no top-event PMHF.
  ASIL B PMHF claims are not made.
- **Common-cause failures (CCF) not analysed.** A production FTA would
  add a dependent-failure analysis (DFA) per ISO 26262-9 clause 7,
  examining e.g. shared power domains between the lamp driver IC and
  the MCU.
- **Hardware tree depth is shallow.** Basic events like "driver IC stuck
  high" are treated as leaves; a production FTA would decompose to
  silicon failure mechanisms (gate-oxide breakdown, electromigration)
  if quantification were required.
- **Software basic events are undeveloped where the cause class is
  "defect".** ISO 26262-6 software unit testing per ASIL is the
  recognised mitigation for these classes; the unit test suite already
  covers many of them but a defect-class basic event is by construction
  unbounded.
- **Security-induced cut sets (B-40) are listed but not quantified
  here.** Security is treated in `doc/security_architecture.md`; ISO
  21434 / SAE J3061 would provide the corresponding TARA.

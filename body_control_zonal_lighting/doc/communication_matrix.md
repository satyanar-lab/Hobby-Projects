# Communication Matrix

**Document scope:** every message that moves through the system, who sends
it, who receives it, when, and over what endpoint.

---

## 1. IP endpoints

### Linux simulation (loopback)

| Process | Binds (recv) | Sends to |
|---|---|---|
| `rear_lighting_node_simulator` | :41001 | :41000 |
| `central_zone_controller_app` | :41000 (rear), :41002 (operator) | :41001 (rear), :41003 (operator) |
| `hmi_control_panel_qt` / `hmi_control_panel_terminal` | :41003 | :41002 |
| `diagnostic_console` | :41003 | :41002 |

### Linux + NUCLEO hardware (real network)

| Node | IP | Recv port |
|---|---|---|
| Linux PC (CZC) | 192.168.0.10 | 41000 (rear service) |
| NUCLEO-H753ZI | 192.168.0.20 | 41001 (rear service) |
| HMI / diag console | 192.168.0.10 | 41003 (operator service) |

Port constants are defined in
`include/body_control/lighting/domain/lighting_service_ids.hpp`.

---

## 2. Rear lighting service messages (`0x5100`)

| # | Message | Producer | Consumer | Trigger | Cycle | Payload | Reliability |
|---|---|---|---|---|---|---|---|
| 1 | `SetLampCommand` | CZC (post-arbitration) | Rear node | HMI/diag intent | Event | 5 B | Reliable |
| 2 | `GetLampStatus` | CZC | Rear node | Consumer status query | Event | 1 B | Reliable |
| 3 | `GetNodeHealth` | CZC | Rear node | Consumer health query | Event | 0 B | Reliable |
| 4 | `LampStatusEvent` | Rear node | CZC | After command applied; also cyclic | 100 ms | 5 B | Best-effort |
| 5 | `NodeHealthStatusEvent` | Rear node | CZC | After health request; also cyclic | 1000 ms | 6 B | Best-effort |

---

## 3. Operator service messages (`0x5200`)

| # | Message | Producer | Consumer | Trigger | Payload | Reliability |
|---|---|---|---|---|---|---|
| 6 | `RequestLampToggle` | HMI / diag console | CZC | User intent | 1 B (`LampFunction`) | Reliable |
| 7 | `RequestLampActivate` | HMI / diag console | CZC | User intent | 1 B | Reliable |
| 8 | `RequestLampDeactivate` | HMI / diag console | CZC | User intent | 1 B | Reliable |
| 9 | `RequestNodeHealth` | HMI / diag console | CZC | User request | 0 B | Reliable |
| 10 | `LampStatusEvent` | CZC | HMI / diag console | After CZC receives from rear node | 5 B (`LampStatus`) | Best-effort |
| 11 | `NodeHealthEvent` | CZC | HMI / diag console | After CZC receives from rear node | 6 B (`NodeHealthStatus`) | Best-effort |

---

## 4. Inter-process view

```
  hmi_control_panel_qt/_terminal         diagnostic_console
        │  OperatorServiceConsumer           │  OperatorServiceConsumer
        │                                    │
        │  UDP :41003 ─────────────────────  │
        └─────────────────┬──────────────────┘
                          │
                          ▼  UDP :41002
         ┌────────────────────────────────────────┐
         │  central_zone_controller_app            │
         │    ├─ OperatorServiceProvider           │
         │    ├─ CommandArbitrator                 │
         │    ├─ LampStateManager                  │
         │    ├─ NodeHealthMonitor                 │
         │    └─ RearLightingServiceConsumer       │
         └──────────────┬─────────────────────────┘
                        │
              ┌─────────┴─────────┐
              │ FanoutTransportAdapter (hardware demo)
              │                   │
              ▼  UDP :41001       ▼  UDP :41001 → 192.168.0.20
  ┌──────────────────────┐  ┌─────────────────────┐
  │  rear_lighting_node_ │  │  NUCLEO-H753ZI       │
  │  simulator (Linux)   │  │  (STM32 bare-metal)  │
  └──────────────────────┘  └─────────────────────┘
```

The CZC's `OperatorServiceProvider` is registered as the CZC's
`SetStatusObserver`, so every `LampStatus` or `NodeHealth` event that
arrives from the rear node is immediately forwarded to all connected
operator clients without the HMI needing to poll.

---

## 5. Timing summary

| Timer | Constant | Value | Purpose |
|---|---|---|---|
| Main loop period | `kMainLoopPeriod` | 20 ms | Periodic tick for health monitor |
| Lamp status publish | `kLampStatusPublishPeriod` | 100 ms | Cyclic `LampStatusEvent` from rear node |
| Node health publish | `kNodeHealthPublishPeriod` | 1000 ms | Cyclic `NodeHealthStatusEvent` from rear node |
| Node heartbeat timeout | `kNodeHeartbeatTimeout` | 2000 ms | Controller declares node `kUnavailable` |
| Indicator blink | `kIndicatorOnTime` / `kIndicatorOffTime` | 500 ms / 500 ms | Indicator blink cadence (STM32 BlinkManager) |
| Hazard blink | `kHazardOnTime` / `kHazardOffTime` | 500 ms / 500 ms | Hazard blink cadence (STM32 BlinkManager) |
| Link debounce | `kLinkDebounceTime` | 100 ms | ETH link supervisor: raw → supervised |

All timing constants are centralised in
`include/body_control/lighting/domain/lighting_constants.hpp`. Do not
introduce new timing values elsewhere.

---

## 6. Endianness & encoding rules

- All multi-byte integer fields are big-endian on the wire.
- Boolean fields are transported as a single byte valued `0x00` or `0x01`.
  The decoder rejects any other value.
- Enums are transported as a single byte.
- The SOME/IP header fields (service ID, method/event ID, client ID,
  session ID) are also big-endian as required by the SOME/IP specification.

---

## 7. Planned future messages

| Message | Producer | Consumer | Purpose |
|---|---|---|---|
| `LampFaultEvent` | Rear node | CZC | Per-lamp fault detail (short / open / over-current) |
| SOME/IP SD `Offer` | Rear node | CZC | Real service discovery once vsomeip SD is configured |
| SOME/IP SD `Find` | CZC | Rear node | Real service discovery |
| UDS `ReadDataByIdentifier` | Diag console | CZC / rear node | ISO 14229 diagnostic reads (Phase 11) |
| OTA `FirmwareChunk` | Head unit | Rear node | Ethernet-based firmware update (Phase 12) |

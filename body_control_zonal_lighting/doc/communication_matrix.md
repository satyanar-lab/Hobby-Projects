# Communication Matrix

**Document scope:** every message that moves through the system, who sends
it, who receives it, when, and over what endpoint.

---

## 1. IP endpoints (Linux simulation)

| Role | Host | UDP port |
|---|---|---|
| Central zone controller | localhost | 41000 |
| Rear lighting node | localhost | 41001 |

Ports are defined in
`src/transport/ethernet/udp_transport_adapter.cpp`. They are not yet
externalised into a config file; that comes in the vsomeip upgrade phase
together with service discovery.

## 2. Message table

| # | Message | Producer | Consumer | Trigger | Cycle | Payload size | Reliability |
|---|---|---|---|---|---|---|---|
| 1 | `SetLampCommand` request | Central zone controller | Rear lighting node | On HMI / diagnostic intent, post-arbitration | Event-driven | 5 B | Reliable |
| 2 | `GetLampStatus` request | Central zone controller | Rear lighting node | On consumer status query | Event-driven | 1 B | Reliable |
| 3 | `GetNodeHealth` request | Central zone controller | Rear lighting node | On consumer health query | Event-driven | 0 B | Reliable |
| 4 | `LampStatusEvent` | Rear lighting node | Central zone controller | After a command is applied; also cyclic | 100 ms | 5 B | Reliable |
| 5 | `NodeHealthStatusEvent` | Rear lighting node | Central zone controller | On `GetNodeHealth`; also cyclic | 1000 ms | 6 B | Reliable |

## 3. Inter-process (in-host, Linux) view

```
              HMI control panel            Diagnostic console
                  │                              │
                  │  (embeds its own consumer)   │
                  ▼                              ▼
              ┌────────────────────────────────────┐
              │  Central zone controller app       │
              │                                    │
              │  RearLightingServiceConsumer       │
              └─────────────┬──────────────────────┘
                            │ UDP 41000 → 41001
                            │ SOME/IP-shaped frames
                            ▼
              ┌────────────────────────────────────┐
              │  Rear lighting node simulator      │
              │                                    │
              │  RearLightingServiceProvider       │
              └────────────────────────────────────┘
```

The HMI and diagnostic console are shown dashed because today they embed
their own `CentralZoneController` + `RearLightingServiceConsumer`. A
later phase will have them connect to the central controller process
over a second service instead.

## 4. Timing summary

| Timer | Constant | Value | Purpose |
|---|---|---|---|
| Main loop period | `kMainLoopPeriod` | 20 ms | Periodic tick for health monitor |
| Lamp status publish | `kLampStatusPublishPeriod` | 100 ms | Cyclic `LampStatusEvent` |
| Node health publish | `kNodeHealthPublishPeriod` | 1000 ms | Cyclic `NodeHealthStatusEvent` |
| Node heartbeat timeout | `kNodeHeartbeatTimeout` | 2000 ms | Controller → declares node `kUnavailable` |
| Indicator on/off time | `kIndicatorOnTime`/`OffTime` | 500 ms / 500 ms | Indicator blink cadence |
| Hazard on/off time | `kHazardOnTime`/`OffTime` | 500 ms / 500 ms | Hazard blink cadence |

All timing constants are centralised in
`include/body_control/lighting/domain/lighting_constants.hpp`. Do not
introduce new timing values elsewhere.

## 5. Endianness & encoding rules

- All multi-byte integer fields are big-endian on the wire.
- Boolean fields are transported as a single byte valued `0x00` or `0x01`.
  The decoder rejects any other value.
- Enums are transported as a single byte.
- Reserved bytes (where the payload codec reserves fixed-size slots) are
  written as `0x00` on encode and ignored on decode.

## 6. Planned future messages (Phase 4+)

| Message | Producer | Consumer | Purpose |
|---|---|---|---|
| `LampFaultEvent` | Rear node | Controller | Per-lamp fault detail (short / open / over-current) |
| `ServiceDiscoveryOffer` | Rear node | Controller | Real SOME/IP SD once vsomeip is in |
| `ServiceDiscoveryFind` | Controller | Rear node | Real SOME/IP SD once vsomeip is in |

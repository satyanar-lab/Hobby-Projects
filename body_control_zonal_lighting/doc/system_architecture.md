# System Architecture

**Document scope:** the layered shape of the software, the dependency rules
between layers, the physical topology of executables, and the Adaptive
AUTOSAR / SDV concepts that informed each choice.

---

## 1. Guiding principles

1. **Feature logic lives once.** Indicator arbitration, lamp state, and
   health monitoring are written once in `application/` and used by every
   executable. The four `main()` files under `app/` are thin wiring, not
   logic.
2. **Layers depend downward only.** `application` depends on `domain`.
   `service` depends on `application`, `transport`, and `domain`.
   `transport` depends on `domain`. Nothing in `domain` depends on
   anything else in the project.
3. **Platform selection is a configure-time concern.** No `#ifdef
   __linux__` scattered through application code. The CMake build selects
   Linux or STM32 platform sources behind the same public headers.
4. **The service interface is the public contract, not the transport.**
   Callers talk to `RearLightingServiceConsumerInterface`, not to UDP or
   vsomeip. Transport is an implementation detail that can be replaced.
5. **Honest failure, not silent success.** Every cross-component call
   returns a status code. When the provider cannot observe a health
   source, it reports `kDegraded`, not `kOperational`.

## 2. Layered view

```
┌──────────────────────────────────────────────────────────────────────┐
│ app/                                                                 │
│   central_zone_controller_app   rear_lighting_node_simulator         │
│   hmi_control_panel             diagnostic_console                   │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ thin wire-up (main.cpp)
┌──────────────────────────────┴───────────────────────────────────────┐
│ hmi/                     application/                                │
│   HmiCommandMapper         CentralZoneController                     │
│   HmiViewModel             CommandArbitrator                         │
│   MainWindow               LampStateManager                          │
│                            NodeHealthMonitor                         │
│                            NodeHealthSourceInterface (abstract)      │
│                            RearLightingFunctionManager               │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ consumes service interface, not transport
┌──────────────────────────────┴───────────────────────────────────────┐
│ service/                                                             │
│   RearLightingServiceConsumerInterface   (public contract)           │
│   RearLightingServiceConsumer            (controller-side facade)    │
│   RearLightingServiceProvider            (node-side facade)          │
│   RearLightingServiceEventListenerInterface                          │
│   DiagnosticServiceInterface                                         │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ service talks to transport abstraction
┌──────────────────────────────┴───────────────────────────────────────┐
│ transport/                                                           │
│   TransportAdapterInterface              (SOME/IP-shaped adapter)    │
│   TransportMessageHandlerInterface                                   │
│   TransportMessage                                                   │
│   SomeipMessageBuilder / SomeipMessageParser                         │
│   ethernet/   UdpTransportAdapter, ethernet_frame_adapter (Linux)    │
│   vsomeip/    runtime manager, client & server adapters              │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ leaf types, no internal deps
┌──────────────────────────────┴───────────────────────────────────────┐
│ domain/                                                              │
│   LampFunction, LampCommandAction, CommandSource, LampCommand        │
│   LampOutputState, LampStatus                                        │
│   NodeHealthState, NodeHealthStatus                                  │
│   lighting_service_ids  (service/method/event IDs)                   │
│   lighting_constants    (timing + sequence ranges)                   │
│   lighting_payload_codec (on-wire encode/decode)                     │
│   domain_type_validators (structural validity checks)                │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
┌──────────────────────────────┴───────────────────────────────────────┐
│ platform/                                                            │
│   linux/   linux_clock, linux_logger, process_signal_handler         │
│   stm32/   gpio_output_driver, ethernet_link_supervisor,             │
│            stm32_diagnostic_logger, nucleo_h753zi_pin_map            │
└──────────────────────────────────────────────────────────────────────┘
```

## 3. Physical topology (Linux phase — Phase 4)

Four OS processes on the same host, connected by two independent UDP
service paths.  The controller is the single decision-making process;
HMI and diagnostic console are thin operator clients that never see the
rear node directly.

```
  ┌──────────────────────┐         ┌──────────────────────────┐
  │  hmi_control_panel   │         │   diagnostic_console     │
  │  OperatorServiceConsumer       │   OperatorServiceConsumer│
  └──────────┬───────────┘         └───────────┬──────────────┘
             │  UDP :41003 ──────────────────── │
             │       (operator client port)     │
             └─────────────────┬────────────────┘
                               │
                               ▼  UDP :41002  (operator server port)
  ┌────────────────────────────────────────────────────────────┐
  │  central_zone_controller_app                               │
  │    ├─ OperatorServiceProvider  (recv :41002, reply :41003) │
  │    ├─ CommandArbitrator                                     │
  │    ├─ LampStateManager / NodeHealthMonitor                  │
  │    └─ RearLightingServiceConsumer  (send :41001)           │
  └───────────────────────────┬────────────────────────────────┘
                               │  UDP :41001  (rear-node client port)
                               ▼  UDP :41000  (rear-node server port)
  ┌────────────────────────────────────────────────────────────┐
  │  rear_lighting_node_simulator                              │
  │    ├─ RearLightingServiceProvider  (recv :41000)           │
  │    ├─ RearLightingFunctionManager                          │
  │    └─ (optional) NodeHealthSource ── feeds health events   │
  └────────────────────────────────────────────────────────────┘
```

**Operator service** (ports 41002 / 41003) carries lamp toggle / activate /
deactivate requests from operator clients to the controller, and
`LampStatus` + `NodeHealth` events back.  Multiple clients may share the
client port because each binds by application ID, not by port.

**Rear lighting service** (ports 41000 / 41001) carries `SetLampCommand`
requests from the controller to the rear node, and `LampStatus` /
`NodeHealth` events back.  This path is internal to the controller subsystem
and is never exposed to operator clients.

**Start order:** the rear node must be up before the controller connects;
the controller must be up before the HMI or diagnostic console send their
first request.

## 4. Transport — what we ship vs. what we're targeting

| Layer | Today (Linux) | Target (Linux) | Target (STM32) |
|---|---|---|---|
| Application code | `RearLightingServiceConsumer/Provider` | Unchanged | Unchanged |
| Transport interface | `TransportAdapterInterface` | Unchanged | Unchanged |
| Wire format | SOME/IP-shaped framing, big-endian, fixed layouts | Real SOME/IP | Same framing over LwIP |
| Backend | Berkeley-sockets UDP on 41000/41001 | vsomeip | LwIP + UDP |
| Service discovery | None — static endpoints | SD via vsomeip | SD-compatible if bridged |

The `transport/vsomeip/` directory holds the facade that will later hold
the real vsomeip integration. Today those translation units delegate to
the ethernet UDP backend so the rest of the system can already be written
against the "runtime" shape.

## 5. Adaptive AUTOSAR / SDV alignment

This is not an Adaptive AUTOSAR implementation, but the shape intentionally
echoes it so the project reads familiarly to anyone who works in that
space:

| Concept | Where it lives here |
|---|---|
| Service-oriented communication | `service/` facades on top of `transport/` |
| Proxy / Skeleton | `RearLightingServiceConsumer` / `...Provider` |
| Event / field subscription | Provider → consumer publishes via `TransportAdapter::SendEvent`, consumer dispatches through `RearLightingServiceEventListenerInterface` |
| Method call (R/R) | `SendLampCommand`, `RequestLampStatus`, `RequestNodeHealth` |
| E2E / determinism concerns | Explicit sequence counters on commands and status |
| Service discovery | Deferred to the vsomeip upgrade phase |
| Execution management | Approximated by each executable's `main()` + Initialize/Shutdown |
| Platform health | `NodeHealthSourceInterface` + `NodeHealthMonitor` |

## 6. Portability contract

The promise is: the Linux and STM32 builds share every file under
`include/body_control/lighting/{domain,application,service,transport,hmi}`
and every file under `src/` except the platform-specific and
transport-backend translation units.

CMake enforces this by splitting source lists into
`BODY_CONTROL_LIGHTING_CORE_SOURCES_COMMON`,
`BODY_CONTROL_LIGHTING_CORE_SOURCES_LINUX`, and
`BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32`, and selecting which set is
compiled based on `BODY_CONTROL_LIGHTING_TARGET_PLATFORM`.

## 7. Known architectural debt (tracked for later phases)

- `SomeipMessageBuilder` / `SomeipMessageParser` hand-roll payload layouts
  that overlap with `lighting_payload_codec`. Either the SOME/IP layer
  should delegate to the codec, or the codec should be retired in favour
  of the builder/parser. Pick one source of truth before Phase 4.
- The `transport/ethernet/udp_transport_adapter.cpp` uses POSIX sockets
  directly; an STM32 port will need an LwIP backend behind the same
  `TransportAdapterInterface`.
- Service discovery is static. The vsomeip upgrade phase will introduce SD.
- `RearLightingServiceConsumer` uses `client_id_` and `next_session_id_`
  members that are wired but not yet round-tripped end-to-end; that gets
  tightened when real request/response correlation is added.

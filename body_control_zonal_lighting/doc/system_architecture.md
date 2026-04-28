# System Architecture

**Document scope:** the layered shape of the software, the dependency rules
between layers, the physical topology of executables, and the Adaptive
AUTOSAR / SDV concepts that informed each choice.

---

## 1. Guiding principles

1. **Feature logic lives once.** Indicator arbitration, lamp state, and
   health monitoring are written once in `application/` and used by every
   executable. The `main()` files under `app/` are thin wiring, not logic.
2. **Layers depend downward only.** `application` depends on `domain`.
   `service` depends on `application`, `transport`, and `domain`.
   `transport` depends on `domain`. Nothing in `domain` depends on
   anything else in the project.
3. **Platform selection is a configure-time concern.** No `#ifdef
   __linux__` scattered through application code. CMake selects Linux or
   STM32 platform sources behind the same public headers.
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
│   hmi_control_panel_qt          hmi_control_panel_terminal           │
│   diagnostic_console            stm32_nucleo_h753zi (bare-metal)     │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ thin wire-up (main.cpp)
┌──────────────────────────────┴───────────────────────────────────────┐
│ hmi/                     application/                                │
│   HmiCommandMapper         CentralZoneController                     │
│   HmiViewModel             CommandArbitrator                         │
│   MainWindow               LampStateManager                          │
│   QmlHmiBridge (Qt6)       NodeHealthMonitor                         │
│                            NodeHealthSourceInterface (abstract)      │
│                            RearLightingFunctionManager               │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ consumes service interface, not transport
┌──────────────────────────────┴───────────────────────────────────────┐
│ service/                                                             │
│   RearLightingServiceConsumerInterface   (rear node contract)        │
│   RearLightingServiceConsumer            (controller-side facade)    │
│   RearLightingServiceProvider            (node-side facade)          │
│   RearLightingServiceEventListenerInterface                          │
│   OperatorServiceProviderInterface       (HMI ↔ controller contract) │
│   OperatorServiceConsumer                (HMI-side facade)           │
│   OperatorServiceProvider                (controller-side facade)    │
│   OperatorServiceEventListenerInterface                              │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ service talks to transport abstraction
┌──────────────────────────────┴───────────────────────────────────────┐
│ transport/                                                           │
│   TransportAdapterInterface              (SOME/IP-shaped adapter)    │
│   TransportMessageHandlerInterface                                   │
│   TransportMessage                                                   │
│   SomeipMessageBuilder / SomeipMessageParser                         │
│   ethernet/   UdpTransportAdapter, DirectUdpTransportAdapter (Linux) │
│   vsomeip/    runtime manager, client & server adapters (Linux)      │
│   lwip/       LwipUdpTransportAdapter (STM32)                        │
└──────────────────────────────────────────────────────────────────────┘
                               ▲
                               │ leaf types, no internal deps
┌──────────────────────────────┴───────────────────────────────────────┐
│ domain/                                                              │
│   LampFunction, LampCommandAction, CommandSource, LampCommand        │
│   LampOutputState, LampStatus                                        │
│   NodeHealthState, NodeHealthStatus                                  │
│   lighting_service_ids  (service/method/event IDs, port constants)   │
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

## 3. Physical topology

Four OS processes on a Linux host, connected by two independent UDP
service paths.  The controller is the single decision-making process;
HMI and diagnostic console are thin operator clients that never see the
rear node directly.

```
  ┌──────────────────────┐         ┌──────────────────────────┐
  │  hmi_control_panel   │         │   diagnostic_console     │
  │  (_qt or _terminal)  │         │   OperatorServiceConsumer│
  │  OperatorServiceConsumer       └───────────┬──────────────┘
  └──────────┬───────────┘                     │
             │  UDP :41003 ─────────────────── │
             │       (operator client port)    │
             └────────────────┬────────────────┘
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
  │  rear_lighting_node_simulator  (Linux)  OR                 │
  │  NUCLEO-H753ZI (bare-metal, 192.168.0.20)                  │
  │    ├─ RearLightingServiceProvider  (recv :41000)           │
  │    ├─ RearLightingFunctionManager                          │
  │    └─ (STM32 only) BlinkManager, GpioOutputDriver          │
  └────────────────────────────────────────────────────────────┘
```

**Operator service** (ports 41002 / 41003) carries lamp toggle / activate /
deactivate requests from operator clients to the controller, and
`LampStatus` + `NodeHealth` events back.

**Rear lighting service** (ports 41000 / 41001) carries `SetLampCommand`
requests from the controller to the rear node, and `LampStatus` /
`NodeHealth` events back.  This path is internal to the controller
subsystem and is never exposed to operator clients.

**On hardware:** the CZC uses a `FanoutTransportAdapter` that forwards
every outgoing rear-lighting command to both the vsomeip/UDP simulator
(if running) and a `DirectUdpTransportAdapter` pointing at the NUCLEO at
`192.168.0.20:41001`.

**Start order:** the rear node must be up before the controller connects;
the controller must be up before the HMI or diagnostic console send their
first request.

## 4. Transport — current state vs. future targets

| Layer | Linux simulation | Linux hardware demo | STM32 node |
|---|---|---|---|
| Application code | `RearLightingServiceConsumer/Provider` | Unchanged | Unchanged |
| Transport interface | `TransportAdapterInterface` | Unchanged | Unchanged |
| Wire format | SOME/IP-shaped framing, big-endian, fixed layouts | Same | Same framing over LwIP |
| Linux backend | vsomeip 3.4.10 (rear lighting) + Berkeley-sockets UDP (operator) | Same + DirectUdpTransportAdapter to NUCLEO | LwIP raw UDP |
| Service discovery | vsomeip routing master on CZC process | Same | SD-compatible framing; no SD daemon |

The `FanoutTransportAdapter` in the CZC `main.cpp` allows both the Linux
simulator and the NUCLEO to receive commands simultaneously, making
mixed hardware/software demos possible without code changes.

## 5. Adaptive AUTOSAR / SDV alignment

This is not an Adaptive AUTOSAR implementation, but the shape intentionally
echoes it:

| Concept | Where it lives here |
|---|---|
| Service-oriented communication | `service/` facades on top of `transport/` |
| Proxy / Skeleton | `RearLightingServiceConsumer` / `...Provider`; same pattern for `OperatorService` |
| Event / field subscription | Provider → consumer publishes via `TransportAdapter::SendEvent`, consumer dispatches through `*EventListenerInterface` |
| Method call (R/R) | `SendLampCommand`, `RequestLampStatus`, `RequestNodeHealth`, `RequestLampToggle` |
| E2E / determinism concerns | Explicit sequence counters on commands and status |
| Service discovery | Partially via vsomeip routing master; full SD deferred |
| Execution management | Approximated by each executable's `main()` + Initialize/Shutdown |
| Platform health | `NodeHealthSourceInterface` + `NodeHealthMonitor` + `EthernetLinkSupervisor` |
| HMI layer | `QmlHmiBridge` marshals vsomeip callbacks to Qt main thread; `MainWindow` + `HmiViewModel` remain transport-agnostic |

The service consumer/provider pattern in this project mirrors ara::com proxy/skeleton semantics from Adaptive AUTOSAR. `RearLightingServiceConsumer` maps to a proxy, `RearLightingServiceProvider` maps to a skeleton, and the `LampStatusEvent`/`NodeHealthStatusEvent` map to ara::com events. In a production Adaptive AUTOSAR system, these facades would be generated from ARXML service descriptions and the transport binding would be an ara::com-compliant middleware such as Vector MICROSAR or ETAS RTA-CAR. This project uses vsomeip directly as the SOME/IP transport, which is one of the allowed bindings under the Adaptive AUTOSAR specification.

## 6. Portability contract

The Linux and STM32 builds share every file under
`include/body_control/lighting/{domain,application,service,transport,hmi}`
and every file under `src/` except the platform-specific and
transport-backend translation units.

CMake enforces this by splitting source lists into
`BODY_CONTROL_LIGHTING_CORE_SOURCES_COMMON`,
`BODY_CONTROL_LIGHTING_CORE_SOURCES_LINUX`, and
`BODY_CONTROL_LIGHTING_CORE_SOURCES_STM32`, and selecting which set is
compiled based on `BODY_CONTROL_LIGHTING_TARGET_PLATFORM`.

The STM32 core source list deliberately excludes `CentralZoneController`
(uses `std::thread`) and all HMI sources (POSIX/display).  Only the domain,
codec, service-provider, application function manager, and platform drivers
are compiled for bare-metal.

## 7. Known architectural debt

- `SomeipMessageBuilder` / `SomeipMessageParser` hand-roll payload layouts
  that overlap with `lighting_payload_codec`. Either the SOME/IP layer
  should delegate to the codec, or the codec should be retired. One source
  of truth should be picked before the vsomeip SD upgrade.
- Service discovery is partially wired: vsomeip routing master runs in
  `central_zone_controller`, but the vsomeip JSON configs do not yet exercise
  SD-based service advertisement and find. Static endpoints are the
  current fallback.
- `RearLightingServiceConsumer` uses `client_id_` and `next_session_id_`
  members that are wired but not yet verified end-to-end for request/response
  correlation with the real vsomeip response path.

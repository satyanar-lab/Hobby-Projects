# AUTOSAR Adaptive ARXML descriptions

## Purpose

These files document, in AUTOSAR R20-11 Adaptive ARXML form, the service
model that the C++ in this repository already implements. They describe:

- The application data types exchanged on the bus (`LampDataTypes.arxml`)
- The two service interfaces (`ExteriorLightingService.arxml`,
  `OperatorService.arxml`)
- The three application software components and how they are wired into a
  composition (`BCL_SoftwareComponent.arxml`)

## Why these files exist

In a production Adaptive AUTOSAR program, the proxy and skeleton classes
on top of `ara::com` would be **auto-generated** from these ARXML files by
an RTE generator (e.g. Vector ISOLAR-A, Elektrobit tresos AdaptiveCore).
The toolchain consumes the service-interface model and produces the C++
binding that application code links against.

This project takes the opposite path: the C++ proxies and skeletons
(`ExteriorLightingServiceConsumer`, `OperatorServiceProvider`, etc.) are
written **by hand** to demonstrate first-principles understanding of the
service-interface and SOME/IP layers underneath. The ARXML in this
directory is the model these hand-written classes would be generated
from, so reviewers can confirm the C++ matches the AUTOSAR contract a
generator would have produced.

## ARXML element to C++ mapping

| ARXML element                                | C++ file / class                                                                 |
|---|---|
| `ExteriorLightingService` methods                | `service::ExteriorLightingServiceConsumerInterface` (in `exterior_lighting_service_interface.hpp`); concrete sender in `ExteriorLightingServiceConsumer` |
| `ExteriorLightingService` events                 | `service::ExteriorLightingServiceEventListenerInterface` callbacks (`OnLampStatusReceived`, `OnNodeHealthStatusReceived`) |
| `OperatorService` methods                    | `service::OperatorServiceProviderInterface` (in `operator_service_interface.hpp`); concrete sender in `OperatorServiceConsumer` |
| `OperatorService` events                     | `service::OperatorServiceEventListenerInterface` callbacks (`OnLampStatusUpdated`, `OnNodeHealthUpdated`) |
| `CentralZoneController` SWC                  | `application::CentralZoneController` class; runtime in `app/central_zone_controller/main.cpp` |
| `ExteriorLightingNode` SWC                       | Linux: `app/exterior_lighting_node_simulator/main.cpp`; STM32: `app/stm32_nucleo_h753zi/main.cpp`; Zephyr: `app/zephyr_nucleo_h753zi/` |
| `HmiClient` SWC                              | `app/hmi_control_panel_qt/main.cpp` (Qt6 GUI), `app/hmi_control_panel_terminal/main.cpp` (terminal fallback), `app/diagnostic_console/main.cpp` |
| `BCL_Composition` assembly connectors        | UDP transport wiring in `src/transport/vsomeip/` and `src/transport/lwip/` |
| `LampCommand` data type                      | `domain::LampCommand` struct (`include/.../lamp_command_types.hpp`) |
| `LampStatus` data type                       | `domain::LampStatus` struct (`include/.../lamp_status_types.hpp`) |
| `NodeHealthStatus` data type                 | `domain::NodeHealthStatus` struct (`include/.../lamp_status_types.hpp`) |
| `LampFaultStatus` data type                  | `domain::LampFaultStatus` struct (`include/.../fault_types.hpp`) |
| `FaultCommand` data type                     | `domain::FaultCommand` struct (`include/.../fault_types.hpp`) |
| `LampFunction`, `LampCommandAction`, `CommandSource`, `LampOutputState`, `NodeHealthState`, `FaultCode`, `FaultAction` enumerations | Scoped `enum class` types in the same domain headers |

## Schema version

AUTOSAR R20-11 Adaptive (XML schema version `AUTOSAR_00050.xsd`). The
namespace declaration is the standard:

```xml
<AUTOSAR xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xmlns="http://autosar.org/schema/r4.0"
         xsi:schemaLocation="http://autosar.org/schema/r4.0 AUTOSAR_00050.xsd">
```

XML well-formedness can be verified with `xmllint --noout *.arxml`.
Full XSD validation against the AUTOSAR schema requires the
`AUTOSAR_00050.xsd` file from an AUTOSAR member release; that file is
not redistributed here.

## Honest scope statement

These files describe **service interfaces and component composition only**.
They are not a full AUTOSAR ECU extract. Specifically out of scope for
this portfolio demonstration:

- Deployment manifests (SOME/IP service-instance manifests, port and
  multicast configuration)
- Execution manifests (process startup, scheduling, restart policy)
- Machine-level configuration (function group state, machine FQN,
  network interface bindings)
- E2E protection profiles, COM-based PDU mappings, SecOC configuration
- Diagnostic Extract (DEXT) for the UDS/DoIP layer — the UDS service
  table is documented in `doc/uds_dtc_documentation.md` instead

The four ARXML files here are sufficient to understand what the C++
implements and how a real ARA::COM toolchain would have generated the
same class skeletons.

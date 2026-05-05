# Item Definition

**ISO 26262-3, clause 5**

---

## 1. Item

**Name:** Exterior Lighting Actuator Node — Rear Zone

**Description:** An embedded electronic control unit (ECU) responsible for
activating and deactivating five exterior lamp outputs (left indicator, right
indicator, hazard lamp, park lamp, headlamp) in the rear lighting zone of the
vehicle. The node receives commands from the Central Zone Controller (CZC) over
a service-oriented Ethernet interface and drives GPIO outputs that control lamp
relay/driver hardware.

**Implementation:** STM32H753ZI microcontroller running Zephyr RTOS v3.x.
Source entry point: `app/zephyr_nucleo_h753zi/main.cpp`.

---

## 2. Item Function

The item shall:

1. Accept lamp activation/deactivation commands from the CZC over SOME/IP UDP
   (port 41001).
2. Apply arbitration rules: hazard lamp commands take priority over individual
   indicator commands; exclusive left/right indicator logic prevents simultaneous
   activation.
3. Drive five GPIO outputs to the commanded state within 50 ms of command receipt.
4. Publish periodic lamp status events and a node health heartbeat to the CZC at
   a 1-second interval (`kNodeHealthPublishPeriod = 1000 ms`).
5. Accept UDS diagnostic requests over DoIP (TCP port 13400): read ECU
   identification, read node health, read/clear DTCs, inject/clear fault
   simulation, initiate OTA firmware update.
6. Perform SOME/IP Service Discovery (AUTOSAR AP_PRS_SOMEIPServiceDiscovery):
   broadcast OfferService for ExteriorLightingService (0x5100) on multicast
   224.244.224.245:30490 every 2 seconds.

---

## 3. Item Boundary

### Inside the boundary (in-scope)

| Element | Description |
|---|---|
| STM32H753ZI SoC | Microcontroller including CPU, flash, RAM, Ethernet MAC |
| Zephyr RTOS + BSP | OS, device tree, GPIO and Ethernet drivers |
| Application firmware | Command arbitrator, function manager, fault manager, OTA handler |
| SOME/IP messaging stack | Custom 20-byte SOME/IP-shaped transport (`src/transport/`) |
| SOME/IP-SD codec | SD offerer (`src/transport/some_ip_sd/`) |
| Ethernet PHY | Physical layer transceiver connected to the MCU |
| GPIO output signals | Five digital outputs driving lamp relay/driver circuits |
| DoIP/UDS diagnostic server | `src/transport/doip_server.cpp`, `src/application/uds_request_handler.cpp` |
| MCUboot bootloader | Signature validation, dual-bank OTA, rollback |

### Outside the boundary (out-of-scope)

| Element | Reason |
|---|---|
| Lamp bulbs and wiring harness | Hardware not controlled by this item |
| Central Zone Controller | Separate item; provides commands as inputs |
| HMI / operator panel | Upstream; interacts with CZC, not this node directly |
| Vehicle CAN bus | Not used in this design; Ethernet only |
| Vehicle power supply | 12 V / 24 V vehicle bus; assumed available |
| Relay/driver ICs | Downstream hardware; not modelled in this item |

---

## 4. Operating Modes

| Mode | Description | Relevant Lamps |
|---|---|---|
| Normal driving | Vehicle moving on road; CZC sends lamp commands as needed | All five |
| Parking | Vehicle stationary; park lamp active, indicators may be used for signalling | Park, indicators |
| Hazard emergency | Driver activates hazard; both indicators flash at 1 Hz regardless of other commands | Hazard + left/right indicator |
| Diagnostic session | UDS session open (service 0x10); fault injection, DTC read, OTA download possible | All (indirectly) |
| OTA update | Firmware transfer via UDS 0x34/0x36/0x37; node reboots into MCUboot after 0x37 | None (lamps hold last state) |
| Post-OTA boot | MCUboot validates new image signature; if valid, swaps and runs; if invalid, rolls back | None until app starts |

---

## 5. Operating Environment

| Parameter | Value | Source |
|---|---|---|
| Ambient temperature | −40 °C to +85 °C (automotive grade) | AEC-Q100 Grade 1 |
| Supply voltage | 12 V nominal (9–16 V range) | Vehicle electrical |
| Communication interface | Ethernet 100BASE-TX; static IP 192.168.0.20 | `prj.conf NET_CONFIG_MY_IPV4_ADDR` |
| Network topology | Point-to-point to CZC (192.168.0.10); no CAN | Architecture constraint |
| Diagnostic access | OBD-II physical layer via DoIP over Ethernet (ISO 13400-2) | TCP port 13400 |
| Firmware update | OTA via UDS over DoIP; image signed ECDSA-P256 | MCUboot + OTA client |
| Vibration / EMC | Vehicle body zone; moderate vibration, 12 V transient immunity required | Not validated in this prototype |

---

## 6. Assumptions of Use

- The CZC is responsible for operator intent interpretation; it validates driver
  inputs before sending commands to this node.
- The CZC enforces the driver's request; this node does not independently decide
  when lamps should be on or off.
- Physical wiring to lamp drivers is assumed correct; this item does not verify
  lamp bulb continuity (no current sensing implemented).
- A development/test Ethernet switch connects the CZC and rear node; production
  would use a deterministic automotive Ethernet switch with QoS configuration.

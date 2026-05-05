# BCZ SOME/IP Wireshark Dissector

`bcz_someip.lua` decodes the Body Control Zonal Lighting custom SOME/IP-shaped
UDP transport on ports 41000–41003.

## Wire format

The BCZ frame uses a 20-byte big-endian header (not the standard 16-byte
SOME/IP header) followed by a variable-length payload:

| Bytes | Field               | Type    | Notes                           |
|-------|---------------------|---------|---------------------------------|
|  0–1  | version             | uint16  | always `0x0001`                 |
|  2–3  | message_kind        | uint16  | 1=REQUEST 2=RESPONSE 3=EVENT    |
|  4–5  | service_id          | uint16  | `0x5100` or `0x5200`            |
|  6–7  | instance_id         | uint16  | always `0x0001`                 |
|  8–9  | method_or_event_id  | uint16  | `0x0xxx` method, `0x8xxx` event |
| 10–11 | client_id           | uint16  |                                 |
| 12–13 | session_id          | uint16  |                                 |
| 14–15 | flags               | uint16  | bit0=is_event bit1=is_reliable  |
| 16–19 | payload_length      | uint32  |                                 |
| 20+   | payload             | bytes   | `payload_length` bytes          |

## UDP port assignments

| Port  | Endpoint                        |
|-------|---------------------------------|
| 41000 | Central Zone Controller (CZC)   |
| 41001 | Exterior Lighting Node          |
| 41002 | Controller Operator endpoint    |
| 41003 | HMI / Diagnostic Console client |

## Decoded services and methods

### ExteriorLightingService (service ID `0x5100`)

| ID       | Name                | Payload                                              |
|----------|---------------------|------------------------------------------------------|
| `0x0001` | SetLampCommand      | LampFunction(1) Action(1) Source(1) SequenceCounter(2) |
| `0x0002` | GetLampStatus       | LampFunction(1)                                      |
| `0x0003` | GetNodeHealth       | *(no payload)*                                       |
| `0x0004` | InjectLampFault     | LampFunction(1) Action(1) Source(1) SequenceCounter(2) |
| `0x0005` | ClearLampFault      | LampFunction(1) Action(1) Source(1) SequenceCounter(2) |
| `0x0006` | GetFaultStatus      | *(no payload)*                                       |
| `0x8001` | LampStatusEvent     | LampFunction(1) OutputState(1) CmdApplied(1) LastSeq(2) |
| `0x8002` | NodeHealthEvent     | HealthState(1) EthLink(1) SvcAvail(1) FaultPresent(1) FaultCount(2) |
| `0x8003` | FaultStatusEvent    | FaultPresent(1) FaultCount(1) FaultCode(2)×N         |

### OperatorService (service ID `0x5200`)

| ID       | Name                   | Payload            |
|----------|------------------------|--------------------|
| `0x0001` | RequestLampToggle      | LampFunction(1)    |
| `0x0002` | RequestLampActivate    | LampFunction(1)    |
| `0x0003` | RequestLampDeactivate  | LampFunction(1)    |
| `0x0004` | RequestNodeHealth      | *(no payload)*     |
| `0x0005` | RequestInjectFault     | LampFunction(1)    |
| `0x0006` | RequestClearFault      | LampFunction(1)    |
| `0x0007` | RequestGetFaultStatus  | *(no payload)*     |
| `0x8001` | LampStatusEvent        | same as ELS above  |
| `0x8002` | NodeHealthEvent        | same as ELS above  |

## Installation

### Wireshark GUI

Copy `bcz_someip.lua` to Wireshark's personal Lua plugin directory, then
restart Wireshark:

```bash
# Linux / WSL
cp bcz_someip.lua ~/.config/wireshark/plugins/

# macOS
cp bcz_someip.lua ~/Library/Application\ Support/Wireshark/plugins/
```

Wireshark will auto-load all `.lua` files in that directory on startup.

### One-shot via tshark

Pass the script directly on the command line — no installation needed:

```bash
tshark \
  -r doc/captures/automotive_ethernet_capture.pcapng \
  -X lua_script:doc/captures/wireshark_dissector/bcz_someip.lua \
  -V 2>&1 | head -60
```

## Usage examples

```bash
# Show all BCZ frames with full field decode
tshark \
  -r doc/captures/automotive_ethernet_capture.pcapng \
  -X lua_script:doc/captures/wireshark_dissector/bcz_someip.lua \
  -Y 'bcz_someip' -V

# Filter to NodeHealthEvent frames only
tshark \
  -r doc/captures/automotive_ethernet_capture.pcapng \
  -X lua_script:doc/captures/wireshark_dissector/bcz_someip.lua \
  -Y 'bcz_someip.method_id == 0x8002' -V

# Summary line per frame (no -V)
tshark \
  -r doc/captures/automotive_ethernet_capture.pcapng \
  -X lua_script:doc/captures/wireshark_dissector/bcz_someip.lua
```

In the Wireshark GUI use display filter `bcz_someip` to show only BCZ
frames, or `bcz_someip.service_id == 0x5100` to narrow to the
ExteriorLightingService.

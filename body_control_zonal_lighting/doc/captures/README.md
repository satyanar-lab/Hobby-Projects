# Wireshark Captures — Automotive Ethernet

This folder contains real packet captures from the running Body Control
Zonal Lighting system showing SOME/IP-shaped UDP traffic and UDS over DoIP
between the Linux controller (192.168.0.10) and the STM32 NUCLEO-H753ZI
exterior lighting node (192.168.0.20).

## Screenshots

### Packet list — DoIP and UDS frames recognized

![Packet list showing DoIP and UDS frames](screenshots/01_packet_list_doip_uds.png)

Wireshark's protocol dissectors automatically identify DoIP (ISO 13400-2) and
UDS (ISO 14229-1) frames in the captured traffic. No custom configuration
required — the framing matches the standards exactly.

### UDS request fully decoded

![UDS request 0x22 ReadDataByIdentifier decoded](screenshots/02_uds_request_decoded.png)

A single UDS request frame expanded through every protocol layer:
Ethernet → IP → TCP → DoIP → UDS. Service ID 0x22 (ReadDataByIdentifier) and
Data Identifier 0xF102 (NodeHealthStatus) are visible at the UDS layer.

### TCP flow graph

![TCP flow graph for UDS exchange](screenshots/03_tcp_flow_graph.png)

Complete TCP exchange visualized: handshake (SYN, SYN+ACK, ACK), DoIP routing
activation, UDS request and response, TCP close (FIN). One full diagnostic
operation in a single picture.

### Protocol hierarchy

![Protocol hierarchy showing DoIP and UDS percentages](screenshots/04_protocol_hierarchy.png)

Statistics view confirming the full automotive protocol stack is present in
the capture and identifying the share of UDS-bearing frames within the overall
TCP traffic.

### SOME/IP-shaped UDP payload

![SOME/IP-shaped UDP frame from rear node](screenshots/05_someip_udp_payload.png)

Frame from the second capture file showing the periodic event traffic on UDP
ports 41000/41001. Wireshark does not auto-decode our custom SOME/IP framing,
so the payload is shown as raw bytes — the codec is documented in
`include/body_control/lighting/transport/lighting_payload_codec.hpp`.

## Capture 1 — automotive_ethernet_capture.pcapng

Controller-to-rear-node periodic communication on ports 41000/41001 over
physical Ethernet. Captured on eth0 with the STM32 board running.

Frame structure observed:

- **Controller → STM32** (port 41000 → 41001), 20-byte payload:
  SOME/IP-shaped GetNodeHealth request (service 0x5100, method 0x0003)
- **STM32 → Controller** (port 41001 → 41000), 26-byte payload:
  NodeHealthStatusEvent response

This proves the SOME/IP-shaped UDP transport is working between the Linux
host and the embedded target over real Ethernet, not just simulated locally.

## Capture 2 — uds_doip_capture.pcapng

UDS diagnostic exchange over DoIP (ISO 13400-2) on TCP port 13400. Captured
while running the Python UDS client against the STM32 board.

Sequence per UDS call:

1. TCP three-way handshake (SYN, SYN+ACK, ACK)
2. DoIP routing activation request and response
3. UDS request — e.g. 0x22 ReadDataByIdentifier for DID 0xF102 (NodeHealthStatus)
4. UDS positive response with payload
5. TCP FIN exchange to close connection

Wireshark's DoIP dissector flags the version byte as "Invalid/unsupported"
because our implementation uses DoIP version 0x01 (the original
ISO 13400-2:2012 revision) while the dissector expects 0x02 (later
revision). Both are valid per the standard.

## How to view

Open either file in Wireshark GUI:

```bash
wireshark doc/captures/automotive_ethernet_capture.pcapng
```

Or analyze from the command line:

```bash
tshark -r doc/captures/uds_doip_capture.pcapng -V | head -100
```

Useful display filters:

| Filter | Shows |
|---|---|
| `doip` | DoIP frames only |
| `uds` | UDS frames only |
| `udp.port == 41001` | SOME/IP-shaped rear-node events |
| `tcp.port == 13400` | All DoIP/UDS TCP traffic |

## Why this matters

These captures are wire-level proof that:

- The system uses actual Ethernet transport, not simulation
- SOME/IP-shaped framing is consistent with the spec
- Real UDS over DoIP is implemented per ISO standards
- The Python diagnostic client speaks the same protocol as a production tester would

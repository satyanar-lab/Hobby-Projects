# Wireshark Captures — Automotive Ethernet

This folder contains real packet captures from the running Body Control
Zonal Lighting system showing SOME/IP-shaped UDP traffic and UDS over DoIP
between the Linux controller (192.168.0.10) and the STM32 NUCLEO-H753ZI
rear lighting node (192.168.0.20).

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

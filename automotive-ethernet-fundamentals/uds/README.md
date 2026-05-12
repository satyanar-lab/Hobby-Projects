# UDS over UDP Demo

A minimal UDS (Unified Diagnostic Services, ISO 14229) client and server
in C++ on Linux, demonstrating the Read Data By Identifier service
(SID 0x22) over a UDP socket bound to the DoIP port 13400.

For simplicity this uses UDP directly rather than the full DoIP framing.
Real diagnostic communication adds the 8-byte DoIP header on top of TCP;
the UDS payload framing shown here is identical.

This folder is part of the larger
[automotive-ethernet-fundamentals](../README.md) project.

---

## Files

- **`uds_client.cpp`** — Sends a 3-byte UDS request `0x22 0xF1 0x90`
  (Read DID 0xF190 = VIN) to 127.0.0.1:13400, then parses the response
  as either positive (0x62) or negative (0x7F).
- **`uds_server.cpp`** — Receives UDS requests, validates the SID and DID,
  and replies with the VIN string for the supported DID or a negative
  response with an appropriate NRC for unknown SIDs/DIDs.

---

## Wire format implemented

### Request (Read Data By Identifier)
Byte 0       Byte 1, 2
[SID 0x22]   [DID big-endian, e.g. 0xF190]

### Positive response
Byte 0       Byte 1, 2     Bytes 3...
[0x62]       [DID echo]    [Data, e.g. 17-byte VIN]

Positive response SID = request SID + 0x40. Universal UDS convention.

### Negative response
Byte 0       Byte 1                Byte 2
[0x7F]       [rejected SID echo]   [NRC]

Supported NRCs:
| NRC  | Meaning                      |
|:-----|:-----------------------------|
| 0x11 | Service not supported         |
| 0x13 | Incorrect message length      |
| 0x31 | Request out of range          |

---

## Build

```bash
g++ -std=c++17 -Wall -Wextra -o uds_server uds_server.cpp
g++ -std=c++17 -Wall -Wextra -o uds_client uds_client.cpp
```

## Run

Terminal 1:
```bash
./uds_server
```

Terminal 2:
```bash
./uds_client
```

Expected output (client):
Positive response (0x62) for DID 0xF190
Data (17 bytes): 56 49 4E 31 32 33 34 35 36 37 38 39 30 41 42 43 44
As ASCII: "VIN1234567890ABCD"

---

## What's missing (honest list)

- **No DoIP framing.** Real diagnostic communication wraps UDS in an
  8-byte DoIP header per ISO 13400-2 and runs over TCP for reliability.
  This demo runs UDS directly over UDP to keep the wire format
  visible without the extra layer.
- **No session control.** UDS 0x10 (DiagnosticSessionControl) and the
  associated default/programming/extended session states are not
  implemented. All requests are treated as one anonymous session.
- **No Security Access.** UDS 0x27 seed/key challenge is not
  implemented. Real ECUs require unlocking before sensitive reads.
- **One service only.** Only 0x22 Read DID is supported; 0x19 Read DTC,
  0x14 Clear DTC, 0x11 ECU Reset, 0x34/0x36/0x37 OTA download, and
  others would be needed for a real diagnostic stack.
- **One DID only.** Only 0xF190 (VIN) is recognized. Real ECUs implement
  dozens of DIDs (software version, hardware version, ECU serial, etc.).
- **No response pending (0x78).** Long operations should return
  0x7F SID 0x78 to keep the client from timing out; not implemented here.

These omissions are deliberate scope limits for a learning project.

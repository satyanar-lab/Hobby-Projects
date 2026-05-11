# SOME/IP Demo

A minimal SOME/IP client and server in C++ on Linux, built from scratch
using only POSIX UDP sockets. No middleware (vsomeip), no ara::com,
no external libraries.

The client constructs a 20-byte SOME/IP REQUEST manually and sends it
to the server. The server parses every field in the 16-byte SOME/IP
header and prints them.

This folder is part of the larger
[automotive-ethernet-fundamentals](../README.md) project.

---

## Files

- **`someip_client.cpp`** — UDP client that manually builds a SOME/IP
  REQUEST (Service 0x5100, Method 0x0001) with a 4-byte payload, sends
  it to `127.0.0.1:5000`, and waits for the reply.
- **`someip_server.cpp`** — UDP server bound to port 5000 that receives
  bytes, validates the minimum length, parses each SOME/IP header field
  with `read_u16_be` / `read_u32_be` helper functions, prints the parsed
  fields, and echoes the raw bytes back.
- **`someip_capture.pcap`** — Wireshark capture of a client → server →
  client round trip. Open in Wireshark and "Decode As" → SOME/IP on
  port 5000 to see the dissected fields.
- **`wireshark_someip_dissected.png`** — Screenshot of Wireshark
  dissecting our message and showing identical field values to the
  server's parser.

---

## SOME/IP header layout

All multi-byte fields are big-endian (network byte order).

| Offset | Size | Field             | Sent value |
|-------:|-----:|:------------------|:-----------|
| 0      | 2 B  | Service ID        | 0x5100     |
| 2      | 2 B  | Method ID         | 0x0001     |
| 4      | 4 B  | Length            | 12         |
| 8      | 2 B  | Client ID         | 0x0001     |
| 10     | 2 B  | Session ID        | 0x0001     |
| 12     | 1 B  | Protocol Version  | 0x01       |
| 13     | 1 B  | Interface Version | 0x01       |
| 14     | 1 B  | Message Type      | 0x00 (REQUEST) |
| 15     | 1 B  | Return Code       | 0x00 (E_OK)    |

**Length field semantic:** the value 12 describes the number of bytes
*after* the Length field — 8 bytes from Client ID through Return Code,
plus 4 bytes of payload.

Payload (4 bytes for this demo, encoding a fictional `LampCommand`):

| Offset | Field           | Sent value |
|-------:|:----------------|:-----------|
| 16     | Lamp function   | 0x01 (LEFT_INDICATOR) |
| 17     | Lamp state      | 0x01 (ON)             |
| 18     | Intensity       | 0x64 (100%)           |
| 19     | Reserved        | 0x00                  |

---

## Design choices

**Manual byte construction, no `htons`/`htonl`.** While `htons`/`htonl`
work, they hide the byte order conversion behind a function call. Doing
it by hand with shifts and masks (e.g.
`(SERVICE_ID >> 8) & 0xFF` for the high byte) makes the big-endian
convention obvious to a reader.

**Pure POSIX sockets, no abstractions.** The point is to see the system
calls a SOME/IP middleware makes underneath. No wrapping, no helper
classes — just `socket()`, `bind()`, `sendto()`, `recvfrom()`, `close()`.

**Defensive error checking.** Every system call's return value is
checked and the error string from `errno`/`strerror` is logged before
the program exits with a non-zero code.

**Helper functions for byte-level parsing.** The server uses two small
helpers (`read_u16_be`, `read_u32_be`) for reading 16-bit and 32-bit
big-endian values out of a `uint8_t` buffer at a given offset. They use
`static_cast` to widen each byte before shifting, avoiding promotion
surprises.

---

## Build

```bash
g++ -std=c++17 -Wall -Wextra -o someip_server someip_server.cpp
g++ -std=c++17 -Wall -Wextra -o someip_client someip_client.cpp
```

The flags `-Wall -Wextra` enable strict warnings — the project compiles
with no warnings on g++ 13.3.

---

## Run

Two terminals.

**Terminal 1 — server:**
```bash
./someip_server
```

Output:
Server bound to port 5000, waiting for messages...

**Terminal 2 — client:**
```bash
./someip_client
```

Client output:
Sending 20 bytes: 51 00 00 01 00 00 00 0C 00 01 00 01 01 01 00 00 01 01 64 00
Received reply: "Q" (20 bytes)

(The reply is garbled when printed as a string because the server
currently echoes raw bytes — adding a proper SOME/IP RESPONSE is on
the to-do list.)

Server output:
Received 20 bytes from 127.0.0.1:46889
Service ID:        0x5100
Method ID:         0x0001
Length:            12
Client ID:         0x0001
Session ID:        0x0001
Protocol Version:  0x01
Interface Version: 0x01
Message Type:      0x00
Return Code:       0x00
Payload (4 bytes): 01 01 64 00

---

## Wireshark capture and dissection

Capture using tcpdump (the `-i any` interface works around WSL2's
loopback quirks):

```bash
sudo tcpdump -i any -w someip_capture.pcap udp port 5000
```

Open `someip_capture.pcap` in Wireshark. By default it shows the packets
as plain UDP. To get the SOME/IP dissection:

1. Right-click any packet → **Decode As...**
2. In the dialog, set UDP port 5000's "Current" column to **SOME/IP**.
3. Click OK.

The Protocol column changes to "SOME/IP" and the dissection tree shows
every field — Service ID, Method ID, Length, Client ID, Session ID,
Protocol Version, Interface Version, Message Type, Return Code, plus
the 4-byte payload as "Data".

Clicking a field in the tree highlights the corresponding bytes in the
hex pane at the bottom. Every byte my client constructed is accounted
for by Wireshark's dissector — independent confirmation that the
implementation matches the AUTOSAR PRS.

---

## What's missing (honest list)

- **No SOME/IP RESPONSE.** The server currently echoes raw bytes back
  rather than constructing a proper RESPONSE message (Message Type 0x80).
- **No Service Discovery.** A real SOME/IP system uses multicast SD
  (`OfferService`, `FindService`, `SubscribeEventgroup`) to find services
  dynamically. Here the client just hardcodes the server address.
- **No events / notifications.** Only the REQUEST pattern is supported.
  Events (Message Type 0x02) and Subscriptions are not implemented.
- **No TCP transport.** SOME/IP supports both UDP and TCP. This demo is
  UDP only.
- **No session tracking.** The Session ID is hardcoded to 0x0001 instead
  of incrementing per request.
- **Hardcoded port and IP.** Both are baked into the source rather than
  configurable via command-line arguments.

These omissions are deliberate scope limits for a learning project, not
oversights.

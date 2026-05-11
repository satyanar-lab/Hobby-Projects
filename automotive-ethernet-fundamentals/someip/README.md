# UDP Echo — Automotive Ethernet Fundamentals

A minimal UDP echo client/server in C++ on Linux. Built from scratch
with raw POSIX sockets to understand the wire-level protocol that
SOME/IP and DoIP run on top of.

## Goal

Learn UDP at the API level: socket creation, binding, sending,
receiving, and address handling. This is foundational for understanding
service-oriented automotive Ethernet protocols.

## Files

- `udp_server.cpp` — receives messages on port 5000, prints them, echoes them back
- `udp_client.cpp` — sends "hello" to the server, prints the reply

## Build

g++ -std=c++17 -Wall -Wextra -o udp_server udp_server.cpp
g++ -std=c++17 -Wall -Wextra -o udp_client udp_client.cpp

## Run

In one terminal:
./udp_server

In another terminal:
./udp_client

## What I learned

- POSIX socket API: `socket()`, `bind()`, `sendto()`, `recvfrom()`
- IPv4 address structures (`sockaddr_in`)
- Network byte order and `htons()` / `ntohs()`
- Loopback addressing (127.0.0.1)
- Why automotive SOME/IP chose UDP for events

## Demo

Client constructs a 20-byte SOME/IP REQUEST and sends it over UDP to port 5000:

    Sending 20 bytes: 51 00 00 01 00 00 00 0C 00 01 00 01 01 01 00 00 01 01 64 00

Server parses the SOME/IP header and prints each field:

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

The header is constructed and parsed manually using shift-and-OR
big-endian serialization — no middleware or external libraries.

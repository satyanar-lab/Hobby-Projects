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

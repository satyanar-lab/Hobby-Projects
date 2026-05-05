# SOME/IP Service Discovery

## 1. Overview

This document describes the SOME/IP-SD (Service Discovery) implementation added
in Phase 15. The implementation follows the
**AUTOSAR AP_PRS_SOMEIPServiceDiscovery** specification and produces frames that
Wireshark's built-in SOME/IP-SD dissector recognises as valid OfferService and
FindService messages.

### Design decisions

| Question | Decision | Rationale |
|---|---|---|
| Wire format | Standard 16-byte SOME/IP header | Wireshark requires 0xFFFF/0x8100 magic bytes — custom 20-byte header would not be recognised |
| Threading (Linux) | Dedicated `std::thread` per class | Existing service-data socket is blocked in `recvfrom`; multiplexing would invasively refactor the hot path |
| Threading (Zephyr) | `k_work_delayable` in main.cpp | No dedicated stack needed; self-rescheduling every 2 s is idiomatic Zephyr |
| Endpoint update | Log-only; static fallback retained | Avoids reinitialising a connected socket at runtime; production would use `SdAwareUdpTransportAdapter` |
| STM32 bare-metal | Not implemented | LwIP IGMP/timer integration cost exceeds portfolio scope; static routing documented as acceptable |
| SD phases | Main phase only (2 s interval) | Initial-wait and repetition phases skipped; Wireshark decodes per-frame with no phase awareness |

---

## 2. Wire format

### 2.1 Standard SOME/IP-SD header (16 bytes)

SOME/IP-SD is the **only** place in this project that uses the standard 16-byte
SOME/IP header.  All service-data traffic uses the project's custom 20-byte header.

```
Byte  0-1  : Service ID  = 0xFFFF
Byte  2-3  : Method  ID  = 0x8100
Byte  4-7  : Length      = total_frame_size - 8
Byte  8-9  : Client  ID  = 0x0000
Byte 10-11 : Session ID  = incrementing counter
Byte    12 : Protocol version = 0x01
Byte    13 : Interface version = 0x01
Byte    14 : Message type = 0x02 (NOTIFICATION)
Byte    15 : Return code  = 0x00
```

### 2.2 OfferService frame (56 bytes total)

```
Byte  0-15 : SOME/IP header (see above)   — Length field = 0x00000030 (48)
Byte    16 : Flags = 0xC0 (Reboot | Unicast)
Byte 17-19 : Reserved = 0x000000
Byte 20-23 : Entries Array Length = 0x00000010 (16)
Byte 24-39 : OfferService Entry (16 bytes)
  Byte 24  : Type          = 0x01 (OfferService)
  Byte 25  : Index First   = 0x00
  Byte 26  : Index Second  = 0x00
  Byte 27  : Num Options   = 0x10  (1 option in run 1, 0 in run 2)
  Byte 28-29 : Service ID  = 0x5100 (ExteriorLightingService)
  Byte 30-31 : Instance ID = 0x0001
  Byte 32    : Major Ver   = 0x01
  Byte 33-35 : TTL         = 0x000005 (5 seconds)
  Byte 36-39 : Minor Ver   = 0x00000000
Byte 40-43 : Options Array Length = 0x0000000C (12)
Byte 44-55 : IPv4 Endpoint Option (12 bytes)
  Byte 44-45 : Length      = 0x0009 (9 bytes after Type field)
  Byte    46 : Type        = 0x04 (IPv4 Endpoint)
  Byte    47 : Reserved    = 0x00
  Byte 48-51 : IPv4 address (e.g. 192.168.0.20)
  Byte    52 : Reserved    = 0x00
  Byte    53 : L4 Protocol = 0x11 (UDP)
  Byte 54-55 : Port        = 0xA029 (41001)
```

### 2.3 FindService frame (44 bytes total)

As OfferService but entry type = `0x00`, major = `0xFF` (any), TTL = 1, minor =
`0xFFFFFFFF` (any), and the options array is empty (options_length = 0).

---

## 3. Port and multicast address assignment

| Constant | Value | Purpose |
|---|---|---|
| Multicast group | 224.244.224.245 | SOME/IP-SD well-known group |
| SD port | 30490 | SOME/IP-SD well-known port |
| Service data (rear node) | 41001 | ExteriorLightingService UDP |
| Service data (CZC) | 41000 | Central Zone Controller UDP |

---

## 4. Implementation

### 4.1 Files

| File | Role |
|---|---|
| `include/.../transport/some_ip_sd_types.hpp` | `ServiceOffer`, `DiscoveredService`, `SdStatus` PODs |
| `include/.../transport/some_ip_sd_codec.hpp` | `EncodeOffer`, `EncodeFind`, `DecodeOffer` declarations |
| `src/transport/some_ip_sd/some_ip_sd_codec.cpp` | Wire format encode/decode |
| `include/.../transport/some_ip_sd_offerer.hpp` | `SomeIpSdOfferer` class |
| `src/transport/some_ip_sd/some_ip_sd_offerer.cpp` | Linux offerer (POSIX sockets + `std::thread`) |
| `include/.../transport/some_ip_sd_listener.hpp` | `SomeIpSdListener` class |
| `src/transport/some_ip_sd/some_ip_sd_listener.cpp` | Linux listener (POSIX sockets + `std::thread`) |
| `app/exterior_lighting_node_simulator/main.cpp` | Starts `SomeIpSdOfferer` at boot |
| `app/central_zone_controller/main.cpp` | Starts `SomeIpSdListener` at boot |
| `app/zephyr_nucleo_h753zi/main.cpp` | Inline `k_work_delayable` SD offerer |
| `test/unit/test_some_ip_sd_codec.cpp` | 14 codec unit tests |

### 4.2 Linux simulator offerer

`SomeIpSdOfferer::Init()` creates a UDP socket with `IP_MULTICAST_TTL=1` and
`IP_MULTICAST_LOOP=1` (so a collocated listener on the same host receives the
offer).  `Start()` launches a `std::thread` that calls `sendto()` to
`224.244.224.245:30490` every `period_ms` (default 2 s).

### 4.3 Linux CZC listener

`SomeIpSdListener::Init()` creates a multicast receive socket with
`SO_REUSEADDR`, binds to `0.0.0.0:30490`, and joins the multicast group with
`IP_ADD_MEMBERSHIP`.  A 1-second `SO_RCVTIMEO` allows the listen loop to check
the `running_` flag without blocking indefinitely.

`Start()` sends a **FindService** (type 0x00) immediately so any provider already
in main phase answers at once, then launches the listen thread.  When a valid
OfferService arrives, the registered `OfferCallback` is invoked with the
`DiscoveredService` (service ID, instance ID, remote IP, remote port).

### 4.4 Zephyr rear-node offerer

A file-scope `K_WORK_DELAYABLE_DEFINE(g_sd_offer_work, SdOfferWorkHandler)` is
scheduled with `k_work_schedule(&g_sd_offer_work, K_MSEC(0))` at the end of
`main()`.  The handler:

1. Lazily creates a UDP socket if not yet open (defers until net stack is ready).
2. Calls `SomeIpSdCodec::EncodeOffer()` with the NUCLEO's static IP (192.168.0.20)
   and service port (41001).
3. Calls `sendto()` to `224.244.224.245:30490`.
4. Reschedules itself with `K_MSEC(2000)`.

No dedicated thread stack is needed.  The Zephyr system work queue executes the
handler.  Flash overhead of the SD codec: < 1 KB (counted in the Zephyr memory
map at FLASH: 31 020 B / 128 KB).

### 4.5 STM32 bare-metal

SOME/IP-SD is **intentionally omitted** on the STM32 bare-metal target.
Supporting outbound multicast on LwIP raw API requires:
- `LWIP_IGMP=1` and `LWIP_MULTICAST_TX_OPTIONS=1` in `lwipopts.h`
- A periodic `igmp_tmr()` call in the main polling loop
- Increased `MEM_SIZE` / `PBUF_POOL_SIZE` to avoid IGMP report drops

This integration cost exceeds the portfolio scope.  The bare-metal target uses
static routing (hardcoded CZC IP 192.168.0.10 / port 41000) and is tagged as
**stm32-fully-working-v1** with that configuration verified on hardware.

---

## 5. Wireshark verification

### 5.1 Capture command

```bash
sudo tshark -i eth0 \
  -w /tmp/someip_sd.pcapng \
  -f "host 224.244.224.245 and port 30490" \
  -a duration:30
```

Run the Linux exterior lighting node simulator during the 30-second capture.
Expected output: one 56-byte UDP frame every 2 seconds addressed to
`224.244.224.245:30490`, plus one 44-byte FindService from the CZC at startup.

### 5.2 Wireshark display filter

```
someipsd
```

Expected columns:
- **Info**: `SOME/IP-SD, OfferService (0x5100, 0x0001)` — one per interval
- **Info**: `SOME/IP-SD, FindService (0x5100, 0x0001)` — one at CZC startup

### 5.3 Same-host testing (Linux loopback)

When CZC and simulator run on the same machine, multicast must loop back through
the loopback interface.  `IP_MULTICAST_LOOP=1` is set on the offerer socket by
default (enabled in `SomeIpSdOfferer::Init()`).  No `ip route` changes required.

### 5.4 Cross-host testing (Linux ↔ NUCLEO)

The Zephyr rear node sends OfferService with `local_ip = 192.168.0.20`,
`local_port = 41001`.  The CZC listener on the Linux host (192.168.0.10)
receives the offer and logs it.  The controller's data socket remains statically
connected to 192.168.0.20:41001 — production would call `SetDiscoveredEndpoint()`
to update the endpoint dynamically.

---

## 6. Production gap

The current implementation demonstrates the SD wire protocol and Wireshark
recognition.  A production integration would also:

1. **Dynamic endpoint update**: replace `DirectUdpTransportAdapter`'s static
   `connect()` with a `SetRemoteEndpoint(ip, port)` method called from the
   listener callback.
2. **TTL-based unavailability**: track the last OfferService timestamp and mark
   the service unavailable after `ttl_seconds` without a refresh.
3. **SD phases**: implement the initial-wait and repetition phases per
   AUTOSAR §4.4.1 (reduces startup latency at cost of ~20 lines per phase).
4. **STM32 multicast**: enable `LWIP_IGMP` and hook `igmp_tmr()` into the
   main polling loop to allow the bare-metal target to join the SD multicast group.

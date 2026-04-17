# Demo Walkthrough — Linux Simulation

This document walks through starting the four-process Linux demo and shows
the expected console output at each step. All four executables communicate
over UDP loopback (127.0.0.1) — no hardware or external library required.

## Port assignments

| Process | Binds | Sends to |
|---|---|---|
| `rear_lighting_node_simulator` | 41001 | 41000 |
| `central_zone_controller_app` | 41000 | 41001 |
| `hmi_control_panel` | 41000 | 41001 |
| `diagnostic_console` | 41000 | 41001 |

The HMI and diagnostic console each create their own `RearLightingServiceConsumer`
and connect directly to the simulator. In a production topology the HMI would
talk only to the central zone controller, but for a single-host demo the
direct path is simpler.

## Step 1 — Start the rear lighting node simulator

```
Terminal A:  ./build/app/rear_lighting_node_simulator
```

Expected output:

```
Rear lighting node simulator is running.
Press ENTER to shut down.
```

The simulator initialises its UDP socket on port 41001 and is now ready to
receive `SetLampCommand` requests and respond with `LampStatus` events.

## Step 2 — Start the central zone controller

```
Terminal B:  ./build/app/central_zone_controller_app
```

Expected output:

```
Central zone controller is running.
Press ENTER to shut down.
```

On startup the controller:
1. Binds its UDP socket (port 41000) and registers itself as the service
   event listener.
2. Sends an initial `GetNodeHealth` request to the simulator.
3. Starts its background health-poll thread, which sends a `GetNodeHealth`
   request every 1 s (`kNodeHealthPublishPeriod`).

The simulator receives the health request and sends back a `NodeHealthEvent`.
The controller's `OnNodeHealthStatusReceived` callback fires and caches
`Operational / eth=up / svc=up / fault=no`.

## Step 3 — Start the HMI control panel

```
Terminal C:  ./build/app/hmi_control_panel
```

Expected output:

```
=== HMI Control Panel ===
1 -> Toggle left indicator
2 -> Toggle right indicator
3 -> Toggle hazard lamp
4 -> Toggle park lamp
5 -> Toggle head lamp
6 -> Request node health
0 -> Exit
Selection:
```

## Step 4 — Toggle the park lamp on

Enter `4` at the HMI prompt.

Expected output after the round-trip completes:

```
--- Lamp Status ---
LeftIndicator  -> Unknown, applied=false, seq=0
RightIndicator -> Unknown, applied=false, seq=0
HazardLamp     -> Unknown, applied=false, seq=0
ParkLamp       -> On, applied=true, seq=1
HeadLamp       -> Unknown, applied=false, seq=0
--- Node Health ---
Operational, eth=up, svc=up, fault=no, fault_count=0
```

What happened on the wire:
1. HMI sends `SetLampCommand(ParkLamp, Activate)` → UDP → simulator (port 41001).
2. Simulator applies the command, sets `ParkLamp = On`, and publishes a
   `LampStatusEvent(ParkLamp, On, applied=true, seq=1)` back to port 41000.
3. HMI's consumer receives the event; `OnLampStatusReceived` updates the
   view model cache. `PrintViewModel` prints the updated state.

## Step 5 — Toggle the hazard lamp on (arbitration test)

Enter `3` at the HMI prompt.

Expected output:

```
--- Lamp Status ---
LeftIndicator  -> Unknown, applied=false, seq=0
RightIndicator -> Unknown, applied=false, seq=0
HazardLamp     -> On, applied=true, seq=2
ParkLamp       -> On, applied=true, seq=1
HeadLamp       -> Unknown, applied=false, seq=0
--- Node Health ---
Operational, eth=up, svc=up, fault=no, fault_count=0
```

Both ParkLamp and HazardLamp are On simultaneously because the command
arbitrator (`CommandArbitrator`) allows both — hazard only blocks the two
indicators, not the park lamp. See `test_hazard_priority_behavior` for the
arbitration rules.

## Step 6 — Request node health explicitly

Enter `6` at the HMI prompt.

Expected output (health fields populated by the simulator's response):

```
--- Node Health ---
Operational, eth=up, svc=up, fault=no, fault_count=0
```

The background health-poll in the central zone controller also fires every
1 s; if the simulator is shut down mid-session the health state will
transition to `Unavailable` within 2 s (`kNodeHeartbeatTimeout`).

## Step 7 — Toggle park lamp off

Enter `4` again.

Expected output:

```
ParkLamp -> Off, applied=true, seq=3
```

## Step 8 — Shut down

Press ENTER in Terminals A and B to stop the simulator and controller.
Enter `0` in Terminal C to stop the HMI. All processes exit cleanly.

---

## Diagnostic console (optional Terminal D)

```
Terminal D:  ./build/app/diagnostic_console
```

The diagnostic console provides explicit activate/deactivate commands for
each lamp function and a direct node-health poll — useful for exercising
specific transitions without going through the toggle logic in the HMI.

Example: enter `7` to activate park lamp, `8` to deactivate it, `11` to
inspect node health.

Expected health output:

```
Node health: Operational, eth=up, svc=up, fault=no, fault_count=0
```

---

## What is not yet wired (Phase 4 / Phase 5)

- **Real vsomeip**: `vsomeip_runtime_manager.cpp` currently delegates to
  the UDP backend. Phase 4 will replace this with actual vsomeip SD and
  client/server routing.
- **STM32 rear node**: Phase 5 will replace `rear_lighting_node_simulator`
  with the NUCLEO-H753ZI target running the GPIO driver and LwIP transport.

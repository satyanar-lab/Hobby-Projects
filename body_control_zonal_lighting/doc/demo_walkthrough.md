# Demo Walkthrough — Linux Simulation

This document walks through starting the four-process Linux demo and shows
the expected console output at each step.  All four processes communicate
over UDP loopback (127.0.0.1) — no hardware or external library required
beyond vsomeip 3.4.10 (for the simulator and controller paths) and
optionally Qt6 (for the GUI HMI).

## Port assignments

| Process | Binds (recv) | Sends to | Role |
|---|---|---|---|
| `rear_lighting_node_simulator` | :41001 | :41000 | Rear lighting service provider |
| `central_zone_controller_app` | :41000 (rear), :41002 (operator) | :41001 (rear), :41003 (operator) | Decision-maker |
| `hmi_control_panel_qt` or `hmi_control_panel_terminal` | :41003 | :41002 | Operator client |
| `diagnostic_console` | :41003 | :41002 | Operator client |

The HMI and diagnostic console communicate exclusively with the CZC over the
operator service path (:41003 ↔ :41002).  Neither client speaks directly to
the rear node.

## Step 1 — Start the rear lighting node simulator

```
Terminal A:  ./build/app/rear_lighting_node_simulator
```

Expected output:

```
Rear lighting node simulator is running.
Press Ctrl+C to shut down.
```

The simulator binds UDP port 41001, starts its periodic publish loop, and
waits for `SetLampCommand` requests.

## Step 2 — Start the central zone controller

```
Terminal B:  ./build/app/central_zone_controller_app
```

Or via the run script (which sets vsomeip env vars):

```
Terminal B:  tools/run_controller.sh
```

Expected output:

```
Central zone controller is running.
Press Ctrl+C to shut down.
```

On startup the controller:
1. Binds rear service socket (:41000) and operator service socket (:41002).
2. Registers `OperatorServiceProvider` as the CZC status observer so every
   incoming `LampStatus` and `NodeHealth` event from the rear node is
   immediately forwarded to all connected operator clients.
3. Calls `RequestNodeHealth()` once to prime the health cache.
4. Starts its background health-poll thread (period = `kNodeHealthPublishPeriod`
   = 1000 ms).

## Step 3 — Start the HMI

### Qt6 GUI (default)

```
Terminal C:  ./build/app/hmi_control_panel_qt
```

An automotive-style dark window opens with five lamp buttons and a node
health bar at the bottom.  The status dot in the top-right corner should
turn green within 1–2 seconds as the CZC availability callback fires.

### Terminal fallback

```
Terminal C:  ./build/app/hmi_control_panel_terminal
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

**Qt HMI:** click the Park Lamp button.

**Terminal HMI:** enter `4`.

What happens on the wire:

1. HMI sends `RequestLampToggle(kParkLamp)` → UDP :41002.
2. CZC `OperatorServiceProvider` receives it, calls
   `CentralZoneController::SendLampCommand(kParkLamp, kToggle)`.
3. `CommandArbitrator` accepts (park lamp is independent of hazard state),
   result = `kAccepted`, 1 command.
4. CZC sends `SetLampCommand(kParkLamp, kActivate)` → UDP :41001.
5. Simulator applies the command, publishes
   `LampStatusEvent(kParkLamp, kOn, applied=true, seq=1)` → UDP :41000.
6. CZC receives the event, updates `LampStateManager`, then the status
   observer (`OperatorServiceProvider`) forwards it → UDP :41003.
7. HMI receives `LampStatusEvent`; `MainWindow::OnLampStatusUpdated` updates
   `HmiViewModel`.  Qt HMI button glows blue; terminal HMI shows:

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

## Step 5 — Toggle the hazard lamp on (arbitration test)

**Qt HMI:** click the Hazard button.

**Terminal HMI:** enter `3`.

The `CommandArbitrator` expands hazard activation into three commands
(hazard, left indicator, right indicator).  Three `LampStatusEvent`
messages flow back, so three `OnLampStatusUpdated` callbacks fire.

Terminal HMI expected output:

```
--- Lamp Status ---
LeftIndicator  -> On, applied=true, seq=2
RightIndicator -> On, applied=true, seq=3
HazardLamp     -> On, applied=true, seq=2
ParkLamp       -> On, applied=true, seq=1
HeadLamp       -> Unknown, applied=false, seq=0
```

Both ParkLamp and HazardLamp (plus both indicators) are On.  Park and head
lamp commands are independent of hazard state; see
`test_hazard_priority_behavior` for the full rule set.

## Step 6 — Try to toggle an indicator while hazard is active

**Qt HMI:** click Left Indicator while hazard is on.

**Terminal HMI:** enter `1`.

The `CommandArbitrator` sees `context.hazard_lamp_active == true` and
rejects the activation.  No `SetLampCommand` reaches the simulator, so no
`LampStatusEvent` fires and the HMI state does not change.

**Qt HMI:** Left Indicator button remains dark.

**Terminal HMI:** the lamp status printout remains unchanged.

## Step 7 — Request node health explicitly

**Terminal HMI:** enter `6`.

```
--- Node Health ---
Operational, eth=up, svc=up, fault=no, fault_count=0
```

The CZC's background health-poll also fires every 1 s.  If the simulator is
shut down, the CZC's `NodeHealthMonitor` transitions to `kUnavailable` within
`kNodeHeartbeatTimeout` (2 s) and the operator service forwards that
transition to all connected clients.

## Step 8 — Shut down

Send `Ctrl+C` to Terminals A and B.  Enter `0` in Terminal C (terminal HMI),
or close the Qt window.  All processes exit cleanly.

---

## Diagnostic console (optional Terminal D)

```
Terminal D:  ./build/app/diagnostic_console
```

The diagnostic console exposes explicit activate/deactivate commands for each
lamp function and a direct node-health poll.  It connects to the same operator
service path as the HMI.

Example: enter `7` to activate park lamp, `8` to deactivate, `11` to inspect
node health.

---

## Smoke test (CI / quick sanity check)

`tools/system_smoke_test.sh` starts the simulator, pipes a single lamp
command into `diagnostic_console`, and asserts the output contains
`ParkLamp.*On` — confirming a full operator-service → CZC → rear-node
→ status-event round-trip over UDP.

```bash
bash tools/system_smoke_test.sh
```

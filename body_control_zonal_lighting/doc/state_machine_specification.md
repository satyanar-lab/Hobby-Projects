# State Machine Specification

**Document scope:** the four state machines that drive user-visible
behaviour. Each state diagram lives next to the component that owns it.

---

## 1. Lamp output state (per function)

Owned by: `RearLightingFunctionManager` on the node side, cached by
`LampStateManager` on the controller side.

```
        (construct)
             │
             ▼
        ┌────────┐     Activate / Toggle(from Off)     ┌────────┐
        │  kOff  │ ──────────────────────────────────▶ │  kOn   │
        │        │ ◀────────────────────────────────── │        │
        └────────┘   Deactivate / Toggle(from On)      └────────┘
             ▲                                              │
             │ Reset                                        │ Reset
             │                                              ▼
             │                                         ┌────────┐
             └──────────────────────────────────────── │kUnknown│
                        (never enters kUnknown during  └────────┘
                         normal operation; only on
                         first construction)
```

**Invariants:**

- Initial construction: node side starts at `kOff`; controller-side
  cache starts at `kUnknown` until the first event arrives.
- `kNoAction` and `kUnknown` function never cause a transition.
- `kToggle` inverts only between `kOn` and `kOff`.

---

## 2. Command arbitration (controller side)

Owned by: `CommandArbitrator`. Executed synchronously on every incoming
request before it becomes a service call.  A hazard command fans out to
three output commands; an indicator command may fan out to two.

```
               ┌──────────────────────────────┐
               │          (new request)       │
               └────────────────┬─────────────┘
                                ▼
               ┌──────────────────────────────┐
               │ Structural validation        │
               │  • enum ranges valid         │
               │  • function != kUnknown      │
               │  • action   != kNoAction     │
               └────────┬───────────────┬─────┘
                   pass │               │ fail
                        ▼               ▼
          ┌─────────────────────────┐  ┌───────────┐
          │  Function branch        │  │ kRejected │
          └──┬──────────┬───────────┘  └───────────┘
             │ hazard   │ indicator L/R activate
             │          ▼
             │   context.hazard_lamp_active?
             │          ├─ yes ──▶  kRejected
             │          │
             │          └─ no  ──▶  context.opposite_indicator_active?
             │                         ├─ yes ──▶ kModified:
             │                         │            [deactivate opposite,
             │                         │             activate requested]
             │                         └─ no  ──▶  kAccepted: [activate]
             ▼
         hazard activate/deactivate (resolves kToggle against context)
             ──▶  kAccepted, 3 commands:
                    [hazard cmd, left-indicator cmd, right-indicator cmd]

(park lamp, head lamp, indicator deactivate: kAccepted, 1 command)
```

**Rule summary:**

1. Hazard activate/deactivate always produces three commands: hazard +
   left indicator + right indicator, all with the same resolved action.
   This keeps all three physically in sync (important for blink phase on
   the STM32).
2. Indicator activation is rejected while hazard is active in context.
3. Indicator deactivation is accepted even while hazard is active.
4. Activating one indicator while the other is active produces
   `kModified`: the opposite indicator is deactivated first, then the
   requested one activated.
5. Park and head lamp commands are independent of hazard state.

---

## 3. Node health (controller side)

Owned by: `NodeHealthMonitor`. The cached `NodeHealthStatus.health_state`
evolves like this:

```
   ┌─────────────┐
   │  kUnknown   │   (initial state; no event yet received)
   └─────┬───────┘
         │ first NodeHealthStatusEvent received
         ▼
   ┌─────────────┐    update: health_state == kDegraded    ┌─────────────┐
   │kOperational │ ───────────────────────────────────────▶│  kDegraded  │
   │             │ ◀─────────────────────────────────────── │             │
   └──┬──────────┘    update: health_state == kOperational └─────┬───────┘
      │                                                          │
      │ update: health_state == kFaulted                         │
      ▼                                                          ▼
   ┌─────────────┐ ◀──────────────────────────────────────────────┘
   │  kFaulted   │
   └─────┬───────┘
         │
         │  heartbeat timeout (kNodeHeartbeatTimeout = 2000 ms)
         │  OR  SetServiceAvailability(false)
         ▼
   ┌─────────────┐
   │kUnavailable │   exits only when a new event arrives with a
   └─────────────┘   different health_state
```

Transitions into `kUnavailable`:

- No `NodeHealthStatusEvent` received for `kNodeHeartbeatTimeout` (2000 ms).
- Transport reports service unavailable via `SetServiceAvailability(false)`.

---

## 4. Service consumer availability (controller side)

Owned by: `RearLightingServiceConsumer`. Reflects transport reachability.

```
         Initialize() success          transport up
   ┌────────┐ ───────────────▶ ┌─────────────┐ ◀────────┐
   │ closed │                  │   open      │          │
   │ (init  │                  │ (ready for  │          │ transport
   │  not   │ ◀─────────────── │  requests)  │          │ down
   │ done)  │   Shutdown()     │             │ ─────────┘
   └────────┘                  └──────┬──────┘
                                      │ transport up/down
                                      ▼
                               (fires OnServiceAvailabilityChanged
                                to the registered listener — today:
                                CentralZoneController, which forwards
                                to OperatorServiceProvider, which
                                fans out to all operator clients)
```

---

## 5. Interaction diagram — "user toggles hazard"

```
   User
     │
     ▼ button press / menu entry
   HMI (Qt or terminal)
     │  MainWindow::ProcessAction(kToggleHazardLamp)
     │  QmlHmiBridge marshals to Qt thread (Qt path only)
     ▼
   OperatorServiceConsumer::RequestLampToggle(kHazardLamp)
     │  sends RequestLampToggle message → UDP :41002
     ▼
   OperatorServiceProvider::HandleLampToggleRequest()
     │  calls CentralZoneController::SendLampCommand(kHazardLamp, kToggle)
     ▼
   CommandArbitrator::Arbitrate()
     │  expands to 3 commands: kHazardLamp, kLeftIndicator, kRightIndicator
     ▼
   RearLightingServiceConsumer::SendLampCommand() × 3
     │  sends SetLampCommand messages → UDP :41001
     ▼
   RearLightingFunctionManager::ApplyCommand() × 3
     │  updates GPIO / output state
     ▼
   RearLightingServiceProvider publishes LampStatusEvent × 3
     │  → UDP :41000
     ▼
   CentralZoneController::OnLampStatusReceived() × 3
     │  updates LampStateManager
     │  notifies OperatorServiceProvider (status observer)
     ▼
   OperatorServiceProvider::PublishLampStatusEvent() × 3
     │  → UDP :41003
     ▼
   OperatorServiceConsumer::OnTransportMessageReceived() × 3
     │  fires OnLampStatusUpdated() on the event listener
     ▼
   MainWindow::OnLampStatusUpdated() → HmiViewModel update
   QmlHmiBridge emits lampStatusChanged signal → QML refreshes
```

The three status events arrive synchronously in the loopback test
environment; on the real UDP path they arrive within one round-trip
time of the command being issued.

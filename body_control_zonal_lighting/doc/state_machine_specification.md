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

- Initial construction sets every managed function to `kOff` on the node
  side and `kUnknown` on the controller-side cache until an event arrives.
- `kNoAction` and `kUnknown` function never cause a transition.
- `kToggle` inverts only between `kOn` and `kOff`; from `kUnknown` it is
  treated as `kActivate`.

## 2. Command arbitration (controller side)

Owned by: `CommandArbitrator`. Executed synchronously on every incoming
request before it becomes a service call.

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
          ┌───────────────────────┐   ┌───────────┐
          │ Function under test?  │   │ kRejected │
          └───┬──────────┬────────┘   └───────────┘
              │ hazard   │ indicator (L/R)
              │ activate │ activate
              ▼          ▼
       ┌──────────┐   ┌────────────────────────────┐
       │ kAccepted│   │ Context.hazard_lamp_active?│
       └──────────┘   └──┬────────────┬────────────┘
                        no            yes
                         ▼             ▼
                  ┌──────────┐   ┌───────────┐
                  │ kAccepted│   │ kRejected │
                  └──────────┘   └───────────┘

           (park, head lamp, indicator-deactivate, hazard-deactivate
            are accepted unconditionally once structurally valid)
```

**Rule summary:**

1. Hazard *activation* has absolute priority; it is always accepted once
   structurally valid.
2. Indicator *activation* (left or right) is rejected while hazard is
   active.
3. Indicator *deactivation* is accepted even while hazard is active, so
   the controller can always clear a stuck indicator.
4. Park lamp and head lamp commands are independent of hazard state.

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
      ▼                                                          │
   ┌─────────────┐                                               │
   │  kFaulted   │ ◀─────────────────────────────────────────────┘
   └─────┬───────┘
         │
         │  heartbeat timeout  OR  SetServiceAvailability(false)
         ▼
   ┌─────────────┐
   │kUnavailable │
   └─────────────┘
```

Transitions into `kUnavailable`:

- No `NodeHealthStatusEvent` received for `kNodeHeartbeatTimeout` (2000 ms).
- Transport reports service unavailable via
  `SetServiceAvailability(false)` while any prior health was known.

Transitions out of `kUnavailable` happen only when a fresh event arrives
with a different `health_state`.

## 4. Service consumer availability (controller side)

Owned by: `RearLightingServiceConsumer`. Reflects the underlying
transport reachability.

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
                                to the registered listener)
```

The consumer does not itself cache node data; it forwards events to
whoever has registered via `SetEventListener` (today: the
`CentralZoneController`, which updates its own `LampStateManager` and
`NodeHealthMonitor`).

## 5. Interaction diagram — "user toggles hazard"

```
   User ──▶ HMI ──▶ Controller ──▶ Consumer ──▶ Transport ──▶ Provider
                                                                  │
                                                                  ▼
                                                   FunctionManager: apply
                                                                  │
                                                                  ▼
                                                     LampStatusEvent (1)
                                                                  │
                                                                  ▼
   Transport ──▶ Consumer ──▶ Listener (Controller) ──▶ StateManager + HMI refresh
```

Ticks (1) and below continue at the `kLampStatusPublishPeriod` cadence so
the controller's cache stays current even without explicit queries.

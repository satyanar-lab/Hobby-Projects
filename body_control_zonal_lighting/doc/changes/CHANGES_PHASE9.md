# Phase 9 — Zephyr RTOS Port, HMI Thread Safety, and Display State Fixes

## Summary

Phase 9 delivers four independent improvements across the firmware and HMI
layers: a Zephyr RTOS port of the rear-zone node replacing the bare-metal
STM32 firmware; turn-signal retention logic in the arbitrator; a permanent
fix for the Qt6 GUI freeze under rapid button presses; and a clean
separation of arrow-display state from raw lamp-command state in the QML
dashboard.

---

## 1 — Zephyr RTOS Port (`app/zephyr_nucleo_h753zi/`)

The rear-zone firmware previously ran as bare-metal HAL code
(`app/stm32_nucleo_h753zi/`).  It has been ported to Zephyr RTOS, giving
the rear node a structured multi-threaded runtime, a devicetree GPIO
binding, and Zephyr's structured logging subsystem.

### Architecture — four threads

| Thread | Stack | Role |
|---|---|---|
| `rx_thread` | 2 048 B | Receives UDP datagrams from CZC; parses lamp commands |
| `lamp_thread` | 1 024 B | Applies GPIO outputs; drives the blink manager |
| `tx_thread` | 2 048 B | Publishes `LampStatus` and `NodeHealth` events to CZC |
| `main` (Zephyr idle) | — | Initialises subsystems; spawns the three threads above |

Inter-thread communication uses a `k_msgq` (Zephyr message queue) between
`rx_thread` and `lamp_thread`, keeping the receive path non-blocking.

### New files

| File | Purpose |
|---|---|
| `app/zephyr_nucleo_h753zi/main.cpp` | Entry point; thread definitions; Zephyr kernel object declarations |
| `app/zephyr_nucleo_h753zi/CMakeLists.txt` | Zephyr CMake application; pulls in `body_control_lighting::core` and the Zephyr platform sources |
| `app/zephyr_nucleo_h753zi/prj.conf` | Kconfig fragment — enables networking, UDP, structured logging, and the GPIO driver |
| `app/zephyr_nucleo_h753zi/boards/nucleo_h753zi.overlay` | Devicetree overlay — assigns PB0/PB2/PB7/PB14 GPIO nodes to the five lamp functions |
| `src/platform/zephyr/zephyr_gpio_driver.hpp/.cpp` | `GpioDriverInterface` implementation backed by Zephyr `gpio_pin_configure` / `gpio_pin_set` |
| `src/platform/zephyr/zephyr_udp_transport.hpp/.cpp` | `TransportAdapterInterface` implementation using Zephyr BSD socket API (`zsock_socket`, `zsock_sendto`, `zsock_recvfrom`) |

### Why Zephyr

- **Thread isolation by design:** separating receive, lamp-apply, and
  transmit into named threads removes the timing dependency on a hand-rolled
  super-loop and makes stack overflows and deadline misses inspectable.
- **Structured logging:** `LOG_INF` / `LOG_WRN` output is tagged by module,
  filterable at compile time, and compatible with Zephyr's RTT/UART backends —
  better than a custom UART logger for a portfolio piece.
- **Devicetree GPIO:** pin assignments live in the `.overlay`, not in
  `#define` macros scattered through `main.cpp`; the driver reads them via
  `DT_NODELABEL`, making board bring-up explicit and auditable.

---

## 2 — Turn Signal Retention (`include/body_control/lighting/application/command_arbitrator.hpp`)

Prior to this phase, activating the hazard lamp while a turn signal was
running would silently drop the turn-signal intent.  When the hazard was
deactivated, the indicator did not resume.

An **input registry** was added to `CommandArbitrator`: each operator's last
requested state per lamp function is tracked independently of the arbitrated
output.  When hazard clears, the arbitrator re-evaluates retained turn-signal
intent and re-enables the indicator automatically — matching the behaviour
drivers expect from a real stalk-and-cancel-switch arrangement.

---

## 3 — HMI Thread Safety: Persistent Poll Timer (`app/hmi_control_panel/`)

### Root cause of the GUI freeze

`QmlHmiBridge::armTimer()` was called once per inbound vsomeip event to
start a single-shot 50 ms coalescing timer.  Under rapid button presses
(e.g. park-lamp toggled 50 times), the controller echoed 50–100 status
events.  Each call to `armTimer()` posted a `QMetaObject::invokeMethod`
lambda to the GUI event queue — one `QMetaCallEvent` allocation per RX
event.  Interleaved with QML rendering events from the animated dashboard,
the GUI event queue grew faster than it could drain, causing the freeze.

### Fix — persistent repeating timer

`armTimer()` was deleted.  The `vsomeip` `On*` callbacks now **only** write
into `pending_` under `pending_mtx_` and return — zero Qt calls, zero
cross-thread event-queue posts from the vsomeip thread.

A persistent 80 ms repeating `QTimer` (started once in the constructor,
running forever on the GUI thread) fires `pollAndUpdate()` at ~12 Hz.
`pollAndUpdate()` takes a mutex-protected snapshot of `pending_`, clears it,
forwards the latest `LampStatus` / `NodeHealthStatus` to `MainWindow` for
the terminal HMI, and emits Q_PROPERTY change signals only on real value
transitions.

Result: GUI event-queue pressure is O(1) — exactly 12 timer ticks per
second regardless of vsomeip RX burst rate.

`MainWindow::ProcessAction` (a non-blocking UDP `sendto`, single-digit
microseconds) is called directly on the GUI thread from the Q_INVOKABLE
toggle methods — no `QtConcurrent::run`, no dedicated worker thread, no
`next_session_id_` data race.

### Key properties of the new design

| Property | Value |
|---|---|
| Cross-thread posts per RX event | 0 |
| GUI wake-ups per second (idle) | ~12 |
| GUI wake-ups per second (50 rapid toggles) | ~12 |
| Display latency (worst case) | 80 ms |
| Mutexes | 1 (`pending_mtx_`, held for microseconds) |
| `QMetaObject::invokeMethod` calls | 0 |

---

## 4 — HMI Display State: `leftArrowActive` / `rightArrowActive`

### Problem

The QML arrows were bound directly to `hmi.leftOn` and `hmi.rightOn`.  When
the hazard was active, the firmware sent only a `kHazardLamp=ON` event — it
did **not** synthesise `kLeftIndicator=ON` / `kRightIndicator=ON` events.
Consequently the left and right arrows did not blink during hazard.
Additionally, pressing hazard while a turn signal was active showed both
states simultaneously, which was visually ambiguous.

### Fix — computed properties in `QmlHmiBridge`

Two new `Q_PROPERTY` values were added to `QmlHmiBridge`:

```cpp
bool leftArrowActive()  const noexcept { return hazard_on_ || left_on_; }
bool rightArrowActive() const noexcept { return hazard_on_ || right_on_; }
```

Both notify via a shared `displayStateChanged()` signal, emitted whenever
`left_on_`, `right_on_`, or `hazard_on_` transitions in `pollAndUpdate()`.

The QML arrows bind to `hmi.leftArrowActive` / `hmi.rightArrowActive` for
their blink animation, while the command buttons retain direct bindings to
`hmi.leftOn` / `hmi.rightOn` / `hmi.hazardOn` (raw lamp state — shows
which function is actually commanded).

This separates **display state** (what the driver should see blinking) from
**input state** (what the stalk/button has requested), following the domain
rule that hazard subsumes individual indicators at the display layer without
altering the retained command.

---

## 5 — Button Styling: Automotive Dark-Navy Theme

The five control buttons in `LightingHmi.qml` were restyled to remove all
lamp-state feedback from the button appearance (state is shown exclusively by
the dashboard indicators) and to adopt a colour scheme consistent with
professional automotive tooling dark themes (Vector CANalyzer, ETAS INCA):

| Element | Value |
|---|---|
| Background (default) | `#1A2332` (dark navy) |
| Background (pressed) | `#243447` |
| Border | `#2E4A6B` (medium blue) |
| Icon colour | `#4A90D9` (professional blue) |
| Label colour | `#8AA8C8` (blue-grey) |
| Press animation | `scale: 0.97`, 80 ms `OutQuad` ease |

Buttons are **pure input controls** — appearance is always identical
regardless of lamp state.  The dashboard indicators (arrows, hazard triangle,
park circle, headlamp icon) are the sole source of state display.

---

## Modified Files

| File | Change |
|---|---|
| `app/hmi_control_panel/qml_hmi_bridge.hpp` | Added `leftArrowActive`/`rightArrowActive` Q_PROPERTYs + `displayStateChanged` signal; extended `PendingState` with full `LampStatus`/`NodeHealthStatus` fields for terminal forwarding; removed `armTimer()`; renamed slot to `pollAndUpdate()` |
| `app/hmi_control_panel/qml_hmi_bridge.cpp` | Replaced single-shot coalescer + `armTimer()` with persistent 80 ms `QTimer`; `On*` callbacks write pending only; removed all `QMetaObject::invokeMethod`; Q_INVOKABLEs call `ProcessAction` directly |
| `app/hmi_control_panel/LightingHmi.qml` | Arrow blink timers use `hmi.leftArrowActive`/`hmi.rightArrowActive`; button component restyled to dark-navy; removed all lamp-state bindings from buttons |
| `include/body_control/lighting/application/command_arbitrator.hpp` | Added input registry for turn-signal retention across hazard activation/deactivation |
| `app/CMakeLists.txt` | Removed `Qt6::Concurrent` (no longer used after QtConcurrent::run removed) |

---

## Build

Zephyr build requires the Zephyr SDK and `west`:

```bash
# Zephyr RTOS target (NUCLEO-H753ZI)
west build -p auto -b nucleo_h753zi \
  app/zephyr_nucleo_h753zi -- \
  -DEXTRA_CONF_FILE=prj.conf
west flash
```

Linux simulation and Qt6 HMI are unchanged:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
ctest --test-dir build
./build/app/hmi_control_panel_qt
```

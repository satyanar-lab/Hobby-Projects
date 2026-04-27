# Phase 8 — Qt6 QML GUI HMI Control Panel

## Summary

Replaced the terminal-based HMI with a Qt6 QML dashboard that renders an
automotive-style dark interface.  The existing service and domain layers are
untouched; Qt is isolated entirely inside `app/hmi_control_panel/`.

---

## New Files

### `app/hmi_control_panel/qml_hmi_bridge.hpp` / `.cpp`

`QmlHmiBridge` is a `QObject` adapter that also implements
`OperatorServiceEventListenerInterface`.  It is registered as the event
listener instead of `MainWindow`; it forwards every callback to `MainWindow`
(keeping `HmiViewModel` authoritative) then emits a Qt signal.

Thread safety: `OnLamp*` / `OnNodeHealth*` arrive on the vsomeip / UDP worker
thread.  Every callback marshals its work into the GUI thread via
`QMetaObject::invokeMethod(this, lambda, Qt::QueuedConnection)` with the
payload copied by value.  `MainWindow` / `HmiViewModel` are only ever
touched from the GUI thread.

`Q_INVOKABLE` command methods (`toggleLeftIndicator`, etc.) are called from
QML on the GUI thread and dispatch directly to `MainWindow::ProcessAction`.

Node health state (`ethUp`, `svcUp`, `faultPresent`, `faultCount`,
`healthState`) and `controllerAvailable` are exposed as `Q_PROPERTY` with
`NOTIFY` signals so QML bindings auto-refresh without polling.

### `app/hmi_control_panel/qt_main.cpp`

Qt6 entry point.  Constructs `QGuiApplication`, `OperatorServiceConsumer`,
`MainWindow`, `QmlHmiBridge`, loads `LightingHmi.qml` from the Qt resource
system via `QQmlApplicationEngine`, and sets the bridge as a context property
(`hmi`) so QML can call its methods and read its properties.  Calls
`operator_service.Shutdown()` when the window closes.

### `app/hmi_control_panel/LightingHmi.qml`

Single-file QML dashboard using inline `component` definitions:

- **Title bar** — project name left, controller online/offline dot + text right.
- **Five lamp buttons** — `LampButton` inline component, one per lamp function.
  - Dark background, colour-coded active glow per lamp:
    amber (`#f59e0b`) for indicators, red (`#ef4444`) for hazard,
    blue (`#3b82f6`) for park, yellow (`#facc15`) for head lamp.
  - `isOutputOn` drives background colour and ON/OFF badge via `Behavior`
    smooth transitions (180 ms).
  - `isActive` (command acknowledged) drives the border glow.
  - Blinker overlay (`SequentialAnimation`) pulses the glow rectangle at 440 ms
    per half-cycle when `isBlinker && isActive`; started/stopped imperatively
    via `onIsActiveChanged` to avoid Qt property-binding conflicts.
  - Hover highlight rectangle (4 % white opacity, 100 ms fade).
- **Node health bar** — ETH, SVC, FAULT coloured dots with labels; health-state
  text; REQUEST button.
- Lamp state cache (`lampOutputOn`, `lampActive` JS objects) refreshed from
  `Connections.onLampStatusChanged` by calling the bridge's
  `Q_INVOKABLE isLampOutputOn` / `isLampCommandActive`; ensures QML reads the
  freshest `HmiViewModel` state after each push event.

### `app/hmi_control_panel/resources.qrc`

Embeds `LightingHmi.qml` into the executable at `qrc:/LightingHmi.qml`.

---

## Modified Files

### `app/CMakeLists.txt`

- Renamed terminal HMI target `hmi_control_panel` → `hmi_control_panel_terminal`
  (source unchanged).
- Added `option(BODY_CONTROL_LIGHTING_BUILD_QT_HMI "Build Qt6 GUI HMI" ON)`.
- When enabled: `find_package(Qt6 COMPONENTS Core Quick Qml REQUIRED)`,
  `qt6_add_resources` for the QRC, `add_executable(hmi_control_panel_qt ...)`,
  `set_target_properties(... AUTOMOC ON)`.
- Qt target links `body_control_lighting::core` + `Qt6::Core/Quick/Qml`.
  Does **not** link `body_control_lighting::project_options` because
  MOC-generated code does not meet the project's `-Wshadow -Wold-style-cast
  -Werror` flags; per-target `-Wall -Wextra -Wno-shadow -Wno-old-style-cast`
  applied instead.

---

## Build Commands

```bash
# Linux Qt6 GUI HMI (default ON)
cmake -S . -B build -DBODY_CONTROL_LIGHTING_BUILD_QT_HMI=ON
cmake --build build -j$(nproc)

# Terminal-only (skip Qt)
cmake -S . -B build -DBODY_CONTROL_LIGHTING_BUILD_QT_HMI=OFF
cmake --build build -j$(nproc)
```

Executables produced:
- `build/app/hmi_control_panel_terminal` — original ncurses-style terminal HMI
- `build/app/hmi_control_panel_qt`       — Qt6 QML dashboard (Phase 8)

---

## Design Decisions

| Decision | Rationale |
|---|---|
| Adapter not subclass | `MainWindow` is `final`; mixing `QObject` into the domain layer violates the project's layering rule |
| Context property, not `qmlRegisterType` | Single process-scoped instance; QML should not construct the bridge |
| `QueuedConnection` + value copy | Callbacks arrive on worker thread; copying POD payload into lambda is the only safe cross-thread pattern |
| `AUTOMOC ON` per-target | Avoids enabling global MOC that would affect GoogleTest and other targets |
| Separate `project_options` | MOC output violates `-Wshadow`/`-Wold-style-cast`; Qt target gets its own warning flags |

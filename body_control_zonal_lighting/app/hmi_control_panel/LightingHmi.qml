import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    title: "Body Control Zonal Lighting — HMI"
    width: 960
    height: 520
    minimumWidth: 840
    minimumHeight: 460
    visible: true
    color: "#0A0E14"

    // ── Theme tokens (Opus spec: layered depth, never pure black/white) ─────
    readonly property color bgRoot:        "#0A0E14"
    readonly property color bgPanel:       "#121821"
    readonly property color bgPanelHi:     "#1A2230"
    readonly property color bgButton:      "#1A1F2E"
    readonly property color bgButtonHover: "#222B3A"
    readonly property color borderSubtle:  "#1E2733"
    readonly property color borderMid:     "#2D3A4D"
    readonly property color textPrimary:   "#E8ECF1"
    readonly property color textSecondary: "#8B95A7"
    readonly property color textMuted:     "#5A6478"
    readonly property color statusOk:      "#3DDC97"
    readonly property color statusWarn:    "#FFB547"
    readonly property color statusFault:   "#FF4D5E"

    // ── Lamp function enum values ────────────────────────────────────────────
    readonly property int kLeftIndicator:  1
    readonly property int kRightIndicator: 2
    readonly property int kHazardLamp:     3
    readonly property int kParkLamp:       4
    readonly property int kHeadLamp:       5

    // ── Lamp state cache (refreshed from QmlHmiBridge via Connections) ──────
    property var lampOutputOn: ({1: false, 2: false, 3: false, 4: false, 5: false})
    property var lampActive:   ({1: false, 2: false, 3: false, 4: false, 5: false})

    // ── Clock + session uptime ───────────────────────────────────────────────
    property string currentTime: "00:00:00"
    property int    uptimeSeconds: 0

    function formatUptime(s) {
        var h   = Math.floor(s / 3600)
        var m   = Math.floor((s % 3600) / 60)
        var sec = s % 60
        return (h > 0 ? h + "h " : "")
             + (m < 10 ? "0" : "") + m + "m "
             + (sec < 10 ? "0" : "") + sec + "s"
    }

    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: {
            root.uptimeSeconds++
            root.currentTime = Qt.formatDateTime(new Date(), "HH:mm:ss")
        }
    }

    Component.onCompleted: {
        root.currentTime = Qt.formatDateTime(new Date(), "HH:mm:ss")
        refreshAllLamps()
    }

    function refreshAllLamps() {
        var on = {}, act = {}
        for (var f = 1; f <= 5; f++) {
            on[f]  = hmi.isLampOutputOn(f)
            act[f] = hmi.isLampCommandActive(f)
        }
        lampOutputOn = on
        lampActive   = act
    }

    Connections {
        target: hmi
        function onLampStatusChanged(lampFunction) {
            console.log("lampStatusChanged:", lampFunction,
                        "on:", hmi.isLampOutputOn(lampFunction),
                        "active:", hmi.isLampCommandActive(lampFunction))
            var on  = Object.assign({}, root.lampOutputOn)
            var act = Object.assign({}, root.lampActive)
            on[lampFunction]  = hmi.isLampOutputOn(lampFunction)
            act[lampFunction] = hmi.isLampCommandActive(lampFunction)
            root.lampOutputOn = on
            root.lampActive   = act
        }
    }

    // ── Subtle dot-grid background texture ───────────────────────────────────
    Canvas {
        anchors.fill: parent
        z: -1
        Component.onCompleted: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#161E2A"
            var step = 32
            for (var x = step; x < width; x += step)
                for (var y = step; y < height; y += step) {
                    ctx.beginPath()
                    ctx.arc(x, y, 0.6, 0, Math.PI * 2)
                    ctx.fill()
                }
        }
    }

    // ── Inline components ────────────────────────────────────────────────────

    // Pill-shaped status chip used in the node health bar.
    component StatusChip : Rectangle {
        id: chip
        property color  dotColor:  root.textMuted
        property string chipLabel: ""
        property bool   slowPulse: false
        property bool   fastPulse: false

        height: 28; radius: 14
        width: chipRow.implicitWidth + 24
        color: root.bgPanel
        border.color: root.borderMid; border.width: 1

        Row {
            id: chipRow
            anchors.centerIn: parent
            spacing: 6

            Rectangle {
                id: dot
                width: 8; height: 8; radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: chip.dotColor
                Behavior on color { ColorAnimation { duration: 200 } }

                SequentialAnimation on opacity {
                    running: chip.slowPulse && !chip.fastPulse
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.35; duration: 1100; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1.0;  duration: 1100; easing.type: Easing.InOutSine }
                }
                SequentialAnimation on opacity {
                    running: chip.fastPulse
                    loops: Animation.Infinite
                    NumberAnimation { to: 0.45; duration: 300; easing.type: Easing.Linear }
                    NumberAnimation { to: 1.0;  duration: 300; easing.type: Easing.Linear }
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: chip.chipLabel
                color: root.textSecondary
                font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 0.8
            }
        }
    }

    // Lamp control button with glow, blink animation, and press feedback.
    component LampButton : Rectangle {
        id: btn

        property int    lampFunc:   0
        property string labelText:  ""
        property string iconText:   ""
        property bool   isBlinker:  false
        property color  lampColor:  "#FF8C00"
        property bool   isOutputOn: false
        property bool   isActive:   false

        radius: 6
        color: btn.isOutputOn ? Qt.darker(btn.lampColor, 5.2) : root.bgButton
        border.color: btn.isOutputOn ? btn.lampColor : root.borderSubtle
        border.width: btn.isOutputOn ? 1.5 : 1

        Behavior on color        { ColorAnimation { duration: 180; easing.type: Easing.InOutQuad } }
        Behavior on border.color { ColorAnimation { duration: 180; easing.type: Easing.InOutQuad } }

        // Outer glow — sits behind the button (z: -1)
        Rectangle {
            z: -1
            anchors.centerIn: parent
            width:  parent.width  + 14
            height: parent.height + 14
            radius: parent.radius + 7
            color:  btn.lampColor
            opacity: btn.isOutputOn ? 0.16 : 0.0
            Behavior on opacity { NumberAnimation { duration: 220 } }
        }

        // 1px inner top-edge "lit edge" highlight
        Rectangle {
            anchors { top: parent.top; left: parent.left; right: parent.right }
            height: 1; radius: btn.radius
            color: "#FFFFFF"
            opacity: btn.isOutputOn ? 0.08 : 0.03
            Behavior on opacity { NumberAnimation { duration: 180 } }
        }

        // 2px active-state bar along the bottom edge
        Rectangle {
            anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
            height: 2
            color: btn.lampColor
            opacity: btn.isOutputOn ? 1.0 : 0.0
            Behavior on opacity { NumberAnimation { duration: 180 } }
        }

        // Blink overlay — imperatively started/stopped to avoid binding conflicts
        Rectangle {
            id: blinkOverlay
            anchors.fill: parent; radius: parent.radius
            color: btn.lampColor; opacity: 0.0
        }
        SequentialAnimation {
            id: blinkAnim
            loops: Animation.Infinite
            NumberAnimation { target: blinkOverlay; property: "opacity"; to: 0.22; duration: 460; easing.type: Easing.InOutSine }
            NumberAnimation { target: blinkOverlay; property: "opacity"; to: 0.0;  duration: 460; easing.type: Easing.InOutSine }
        }
        onIsOutputOnChanged: {
            if (btn.isBlinker && btn.isOutputOn) { blinkAnim.start() }
            else { blinkAnim.stop(); blinkOverlay.opacity = 0.0 }
        }

        // Press flash feedback — brief white highlight, no scale (Opus: cars feel snappy)
        Rectangle {
            id: pressFlash
            anchors.fill: parent; radius: parent.radius
            color: "#FFFFFF"; opacity: 0.0
            NumberAnimation {
                id: pressAnim
                target: pressFlash; property: "opacity"
                from: 0.12; to: 0.0; duration: 200; easing.type: Easing.OutQuad
            }
        }

        // Button content — icon + label + ON/OFF badge
        Column {
            anchors.centerIn: parent
            spacing: 8

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: btn.iconText
                font.pixelSize: 32
                color: btn.isOutputOn ? btn.lampColor : root.textMuted
                Behavior on color { ColorAnimation { duration: 180 } }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: btn.labelText
                color: btn.isOutputOn ? root.textPrimary : root.textSecondary
                font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 1.5
                horizontalAlignment: Text.AlignHCenter
                Behavior on color { ColorAnimation { duration: 180 } }
            }

            // ON / OFF pill badge
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 46; height: 18; radius: 9
                color: btn.isOutputOn ? Qt.darker(btn.lampColor, 3.8) : "#0D1218"
                border.color: btn.isOutputOn ? btn.lampColor : root.textMuted
                border.width: 1
                Behavior on color { ColorAnimation { duration: 180 } }

                Text {
                    anchors.centerIn: parent
                    text: btn.isOutputOn ? "ON" : "OFF"
                    color: btn.isOutputOn ? btn.lampColor : root.textMuted
                    font.pixelSize: 9; font.weight: Font.DemiBold; font.letterSpacing: 1.2
                }
            }
        }

        // Mouse area: hover tint + press flash + command dispatch
        MouseArea {
            id: ma; anchors.fill: parent; hoverEnabled: true
            onClicked: {
                pressAnim.restart()
                if      (btn.lampFunc === 1) hmi.toggleLeftIndicator()
                else if (btn.lampFunc === 2) hmi.toggleRightIndicator()
                else if (btn.lampFunc === 3) hmi.toggleHazardLamp()
                else if (btn.lampFunc === 4) hmi.toggleParkLamp()
                else if (btn.lampFunc === 5) hmi.toggleHeadLamp()
            }
            Rectangle {
                anchors.fill: parent; radius: parent.parent.radius
                color: "#FFFFFF"
                opacity: ma.containsMouse && !ma.pressed ? 0.04 : 0.0
                Behavior on opacity { NumberAnimation { duration: 120 } }
            }
        }
    }

    // ── Main layout ───────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header bar ────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: root.bgPanelHi

            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: root.borderSubtle
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 24; rightMargin: 24 }
                spacing: 0

                // Version pill
                Rectangle {
                    width: 54; height: 22; radius: 11
                    color: root.bgPanel
                    border.color: root.borderMid; border.width: 1
                    Text {
                        anchors.centerIn: parent
                        text: "BCL v8"
                        color: root.textMuted
                        font.pixelSize: 9; font.weight: Font.Medium; font.letterSpacing: 0.8
                    }
                }

                Item { width: 16 }

                Text {
                    text: "BODY CONTROL ZONAL LIGHTING"
                    color: root.textSecondary
                    font.pixelSize: 13; font.weight: Font.Medium; font.letterSpacing: 2.5
                }

                Item { Layout.fillWidth: true }

                // Controller availability chip
                Rectangle {
                    height: 28; width: 158; radius: 14
                    color: root.bgPanel
                    border.color: hmi.controllerAvailable ? "#1D3D2A" : "#3D1D22"
                    border.width: 1
                    Behavior on border.color { ColorAnimation { duration: 300 } }

                    Row {
                        anchors.centerIn: parent; spacing: 7

                        Rectangle {
                            width: 8; height: 8; radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: hmi.controllerAvailable ? root.statusOk : root.statusFault
                            Behavior on color { ColorAnimation { duration: 300 } }
                            SequentialAnimation on opacity {
                                running: hmi.controllerAvailable
                                loops: Animation.Infinite
                                NumberAnimation { to: 0.35; duration: 1100; easing.type: Easing.InOutSine }
                                NumberAnimation { to: 1.0;  duration: 1100; easing.type: Easing.InOutSine }
                            }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: hmi.controllerAvailable ? "CONTROLLER: ONLINE" : "CONTROLLER: OFFLINE"
                            color: hmi.controllerAvailable ? root.statusOk : root.statusFault
                            font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 0.8
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                    }
                }
            }
        }

        // ── Content area ──────────────────────────────────────────────────────
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // Subtle top-down car silhouette in the background.
            // Lamp positions: front corners = indicators, rear = park/head.
            Canvas {
                id: carCanvas
                anchors.centerIn: parent
                width: 540; height: 200
                z: 0
                opacity: 0.07
                Component.onCompleted: requestPaint()

                onPaint: {
                    var ctx = getContext("2d")
                    ctx.clearRect(0, 0, width, height)
                    ctx.strokeStyle = "#8B95A7"
                    ctx.lineWidth = 1.5

                    function rr(x, y, w, h, r) {
                        ctx.beginPath()
                        ctx.moveTo(x + r, y)
                        ctx.lineTo(x + w - r, y)
                        ctx.arcTo(x + w, y,     x + w, y + r,     r)
                        ctx.lineTo(x + w, y + h - r)
                        ctx.arcTo(x + w, y + h, x + w - r, y + h, r)
                        ctx.lineTo(x + r, y + h)
                        ctx.arcTo(x,     y + h, x,     y + h - r, r)
                        ctx.lineTo(x,     y + r)
                        ctx.arcTo(x,     y,     x + r, y,         r)
                        ctx.closePath()
                        ctx.stroke()
                    }

                    rr(65, 14, 410, 172, 18)  // car body
                    rr(130, 24, 280, 152, 8)  // cabin / roof
                    rr(18, 28, 42, 54, 8)     // front-left wheel
                    rr(480, 28, 42, 54, 8)    // front-right wheel
                    rr(18, 118, 42, 54, 8)    // rear-left wheel
                    rr(480, 118, 42, 54, 8)   // rear-right wheel
                    rr(68, 17, 50, 20, 3)     // front-left light
                    rr(422, 17, 50, 20, 3)    // front-right light
                    rr(68, 163, 50, 20, 3)    // rear-left light
                    rr(422, 163, 50, 20, 3)   // rear-right light
                }
            }

            // Section label
            Text {
                anchors { top: parent.top; topMargin: 14; horizontalCenter: parent.horizontalCenter }
                text: "EXTERIOR LIGHTING CONTROL"
                color: root.textMuted
                font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 2.2
                z: 1
            }

            // Lamp buttons — z:1 so they render above the car canvas
            RowLayout {
                anchors {
                    verticalCenter: parent.verticalCenter
                    left: parent.left; right: parent.right
                    leftMargin: 24; rightMargin: 24
                }
                spacing: 16
                z: 1

                LampButton {
                    Layout.fillWidth: true; height: 162
                    lampFunc: 1; labelText: "LEFT\nINDICATOR"; iconText: "◁"
                    isBlinker: true; lampColor: "#FF8C00"
                    isOutputOn: root.lampOutputOn[1] === true
                    isActive:   root.lampActive[1]   === true
                }
                LampButton {
                    Layout.fillWidth: true; height: 162
                    lampFunc: 2; labelText: "RIGHT\nINDICATOR"; iconText: "▷"
                    isBlinker: true; lampColor: "#FF8C00"
                    isOutputOn: root.lampOutputOn[2] === true
                    isActive:   root.lampActive[2]   === true
                }
                LampButton {
                    Layout.fillWidth: true; height: 162
                    lampFunc: 3; labelText: "HAZARD"; iconText: "⚠"
                    isBlinker: true; lampColor: "#FF4D5E"
                    isOutputOn: root.lampOutputOn[3] === true
                    isActive:   root.lampActive[3]   === true
                }
                LampButton {
                    Layout.fillWidth: true; height: 162
                    lampFunc: 4; labelText: "PARK\nLAMP"; iconText: "◎"
                    isBlinker: false; lampColor: "#00CC44"
                    isOutputOn: root.lampOutputOn[4] === true
                    isActive:   root.lampActive[4]   === true
                }
                LampButton {
                    Layout.fillWidth: true; height: 162
                    lampFunc: 5; labelText: "HEAD\nLAMP"; iconText: "☀"
                    isBlinker: false; lampColor: "#C8E4FF"
                    isOutputOn: root.lampOutputOn[5] === true
                    isActive:   root.lampActive[5]   === true
                }
            }
        }

        // ── Footer / node health bar ──────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 56
            color: root.bgPanelHi

            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 1; color: root.borderSubtle
            }

            RowLayout {
                anchors { fill: parent; leftMargin: 24; rightMargin: 24 }
                spacing: 16

                Text {
                    text: "NODE HEALTH"
                    color: root.textMuted
                    font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 2.0
                }

                StatusChip {
                    dotColor:  hmi.ethUp ? root.statusOk : root.statusFault
                    chipLabel: "ETH: " + (hmi.ethUp ? "UP" : "DOWN")
                    slowPulse: hmi.ethUp; fastPulse: false
                }

                StatusChip {
                    dotColor:  hmi.svcUp ? root.statusOk : root.statusFault
                    chipLabel: "SVC: " + (hmi.svcUp ? "UP" : "DOWN")
                    slowPulse: hmi.svcUp; fastPulse: false
                }

                StatusChip {
                    dotColor:  hmi.faultPresent ? root.statusFault : root.statusOk
                    chipLabel: hmi.faultPresent
                               ? ("FAULT: " + hmi.faultCount) : "FAULT: NONE"
                    slowPulse: !hmi.faultPresent; fastPulse: hmi.faultPresent
                }

                // Health state text
                Text {
                    readonly property var names:  ["UNKNOWN", "OPERATIONAL", "DEGRADED", "FAULTED", "UNAVAILABLE"]
                    readonly property var colors: [root.textMuted, root.statusOk,
                                                   root.statusWarn, root.statusFault, root.statusFault]
                    text:  names[Math.min(hmi.healthState, 4)]
                    color: colors[Math.min(hmi.healthState, 4)]
                    font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.5
                    Behavior on color { ColorAnimation { duration: 200 } }
                }

                Item { Layout.fillWidth: true }

                // REQUEST button
                Rectangle {
                    width: 84; height: 28; radius: 6
                    color: reqArea.containsMouse ? root.bgButton : "transparent"
                    border.color: root.borderMid; border.width: 1
                    Behavior on color { ColorAnimation { duration: 100 } }
                    Text {
                        anchors.centerIn: parent
                        text: "REQUEST"
                        color: root.textSecondary
                        font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 1.2
                    }
                    MouseArea {
                        id: reqArea; anchors.fill: parent; hoverEnabled: true
                        onClicked: hmi.requestNodeHealth()
                    }
                }

                // Divider
                Rectangle { width: 1; height: 28; color: root.borderSubtle }

                // Clock + uptime
                Column {
                    spacing: 3
                    Text {
                        anchors.right: parent.right
                        text: root.currentTime
                        color: root.textSecondary
                        font.family: "Consolas, Courier New, monospace"
                        font.pixelSize: 13; font.weight: Font.Medium; font.letterSpacing: 0.5
                    }
                    Text {
                        anchors.right: parent.right
                        text: "UP " + root.formatUptime(root.uptimeSeconds)
                        color: root.textMuted
                        font.family: "Consolas, Courier New, monospace"
                        font.pixelSize: 9; font.letterSpacing: 0.4
                    }
                }
            }
        }
    }
}

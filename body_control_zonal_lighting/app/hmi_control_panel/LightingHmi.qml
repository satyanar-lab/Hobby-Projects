import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    title: "Body Control — Zonal Lighting"
    width: 1040
    height: 610
    minimumWidth: 920
    minimumHeight: 540
    visible: true
    color: "#070B10"

    // ── Theme ─────────────────────────────────────────────────────────────────
    readonly property color clAmber:   "#FF8C00"
    readonly property color clGreen:   "#00CC44"
    readonly property color clWhite:   "#C8E8FF"
    readonly property color clRed:     "#FF3344"
    readonly property color clPanel:   "#0D1117"
    readonly property color clBorder:  "#1E2D40"
    readonly property color clBorderHi:"#2A3A5C"
    readonly property color clText:    "#C8D4E0"
    readonly property color clDim:     "#2A3A4C"
    readonly property color clDimText: "#506070"

    // ── Clock — only JS timer; zero lamp-state JS variables ───────────────────
    property string clockTime: Qt.formatDateTime(new Date(), "HH:mm:ss")
    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: root.clockTime = Qt.formatDateTime(new Date(), "HH:mm:ss")
    }

    // ── Carbon dot-grid background ────────────────────────────────────────────
    Canvas {
        anchors.fill: parent; z: -1
        Component.onCompleted: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.clearRect(0, 0, width, height)
            ctx.fillStyle = "#111C28"
            var step = 20
            for (var x = step; x < width; x += step)
                for (var y = step; y < height; y += step) {
                    ctx.beginPath()
                    ctx.arc(x, y, 0.5, 0, Math.PI * 2)
                    ctx.fill()
                }
        }
    }

    // ── LampBtn component ─────────────────────────────────────────────────────
    component LampBtn : Rectangle {
        id: lb
        property int    fn:    0
        property string icon:  ""
        property string label: ""

        height: 65; radius: 8
        color: btnArea.pressed ? "#243447" : "#1A2332"
        border.color: "#2E4A6B"; border.width: 1
        scale: btnArea.pressed ? 0.97 : 1.0
        Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutQuad } }

        Column {
            anchors.centerIn: parent
            spacing: 5
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: lb.icon
                font.pixelSize: 22
                color: "#4A90D9"
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: lb.label
                color: "#8AA8C8"
                font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 1.4
            }
        }

        MouseArea {
            id: btnArea; anchors.fill: parent
            onClicked: {
                if      (lb.fn === 1) hmi.toggleLeftIndicator()
                else if (lb.fn === 2) hmi.toggleRightIndicator()
                else if (lb.fn === 3) hmi.toggleHazardLamp()
                else if (lb.fn === 4) hmi.toggleParkLamp()
                else if (lb.fn === 5) hmi.toggleHeadLamp()
            }
        }
    }

    // ── Main layout ───────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Top bar ───────────────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true
            height: 48
            color: root.clPanel
            Rectangle {
                anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
                height: 1; color: root.clBorderHi
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 20; rightMargin: 20 }
                spacing: 0

                Rectangle {
                    width: 56; height: 22; radius: 11
                    color: "#070B10"
                    border.color: root.clBorder; border.width: 1
                    Text {
                        anchors.centerIn: parent; text: "BCL v9"
                        color: root.clDimText
                        font.pixelSize: 9; font.weight: Font.Medium; font.letterSpacing: 0.8
                    }
                }
                Item { width: 14 }
                Text {
                    text: "BODY CONTROL  ·  ZONAL LIGHTING"
                    color: root.clDimText
                    font.pixelSize: 12; font.weight: Font.Medium; font.letterSpacing: 2.6
                }
                Item { Layout.fillWidth: true }

                // Controller status dot + label
                Item {
                    // blinkState for the controller dot
                    property bool blinkState: true
                    implicitWidth: ctrlRow.implicitWidth
                    implicitHeight: ctrlRow.implicitHeight

                    Row {
                        id: ctrlRow
                        spacing: 8; anchors.verticalCenter: parent.verticalCenter
                        Rectangle {
                            id: ctrlDot
                            width: 8; height: 8; radius: 4
                            anchors.verticalCenter: parent.verticalCenter
                            color: hmi.controllerOnline ? root.clGreen : root.clRed
                            opacity: hmi.controllerOnline
                                     ? (parent.parent.blinkState ? 1.0 : 0.25)
                                     : 1.0
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: hmi.controllerOnline ? "CONTROLLER  ONLINE" : "CONTROLLER  OFFLINE"
                            color: hmi.controllerOnline ? root.clGreen : root.clRed
                            font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 1.0
                            Behavior on color { ColorAnimation { duration: 300 } }
                        }
                    }

                    Timer {
                        interval: 900; repeat: true
                        running: hmi.controllerOnline
                        onTriggered:     parent.blinkState = !parent.blinkState
                        onRunningChanged: if (!running) parent.blinkState = true
                    }
                }

                Item { width: 20 }
                Text {
                    text: root.clockTime
                    color: root.clText
                    font.pixelSize: 15; font.weight: Font.Medium
                    font.family: "Consolas, Courier New, monospace"
                }
            }
        }

        // ── Middle row ────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Instrument cluster ────────────────────────────────────────────
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                // Drop shadow
                Rectangle {
                    anchors.centerIn: cluster
                    width: cluster.width + 18; height: cluster.height + 18
                    radius: cluster.radius + 5
                    color: "#000000"; opacity: 0.55
                }

                Rectangle {
                    id: cluster
                    anchors.centerIn: parent
                    width:  Math.min(parent.width - 36, 740)
                    height: Math.min(parent.height - 24, 296)
                    radius: 20
                    border.color: root.clBorderHi; border.width: 1

                    gradient: Gradient {
                        orientation: Gradient.Vertical
                        GradientStop { position: 0.0; color: "#0D1117" }
                        GradientStop { position: 1.0; color: "#1C2333" }
                    }

                    Rectangle {
                        anchors { fill: parent; margins: 1 }
                        radius: parent.radius - 1; color: "transparent"
                        border.color: "#FFFFFF"; border.width: 1; opacity: 0.04
                    }

                    RowLayout {
                        anchors { fill: parent; margins: 18 }
                        spacing: 10

                        // ── Left turn arrow ───────────────────────────────────
                        Item {
                            id: leftArrowItem
                            Layout.preferredWidth: 86
                            Layout.fillHeight: true

                            // Timer toggles this; opacity binding reads it directly.
                            property bool blinkState: true

                            Column {
                                anchors.centerIn: parent; spacing: 6
                                Text {
                                    id: leftChevron
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "❮❮"; font.pixelSize: 40; color: root.clAmber
                                    opacity: hmi.leftArrowActive
                                             ? (leftArrowItem.blinkState ? 1.0 : 0.15)
                                             : 0.12
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "L"
                                    font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 3.0
                                    color: root.clDim
                                }
                            }

                            Timer {
                                interval: 500; repeat: true
                                running: hmi.leftArrowActive
                                onTriggered:      leftArrowItem.blinkState = !leftArrowItem.blinkState
                                onRunningChanged: if (!running) leftArrowItem.blinkState = true
                            }
                        }

                        // ── Warning icon column ───────────────────────────────
                        Item {
                            id: iconCol
                            Layout.preferredWidth: 64
                            Layout.fillHeight: true

                            // Hazard breathe toggle
                            property bool hazardBlink: true

                            Column {
                                anchors.centerIn: parent; spacing: 14

                                // PARK
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    width: 32; height: 32; radius: 16
                                    color:        hmi.parkOn ? "#003315" : "transparent"
                                    border.color: hmi.parkOn ? root.clGreen : root.clDim
                                    border.width: 1.5
                                    Behavior on color        { ColorAnimation { duration: 180 } }
                                    Behavior on border.color { ColorAnimation { duration: 180 } }
                                    Text {
                                        anchors.centerIn: parent; text: "P"
                                        font.pixelSize: 14; font.weight: Font.Bold
                                        color: hmi.parkOn ? root.clGreen : root.clDim
                                        Behavior on color { ColorAnimation { duration: 180 } }
                                    }
                                }

                                // HAZARD — steady amber, breathes gently when active
                                Text {
                                    id: hazardIcon
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "△"; font.pixelSize: 28
                                    color: hmi.hazardOn ? root.clAmber : root.clDim
                                    opacity: hmi.hazardOn
                                             ? (iconCol.hazardBlink ? 1.0 : 0.5)
                                             : 1.0
                                    Behavior on color { ColorAnimation { duration: 180 } }
                                }

                                // HEADLAMP
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "≡⊳"; font.pixelSize: 18
                                    color: hmi.headOn ? root.clWhite : root.clDim
                                    Behavior on color { ColorAnimation { duration: 180 } }
                                }
                            }

                            Timer {
                                interval: 700; repeat: true
                                running: hmi.hazardOn
                                onTriggered:      iconCol.hazardBlink = !iconCol.hazardBlink
                                onRunningChanged: if (!running) iconCol.hazardBlink = true
                            }
                        }

                        // ── Speedometer ───────────────────────────────────────
                        Item {
                            Layout.fillWidth: true
                            Layout.fillHeight: true

                            Canvas {
                                id: speedo
                                width: 220; height: 220
                                anchors.centerIn: parent
                                Component.onCompleted: requestPaint()

                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.clearRect(0, 0, width, height)
                                    var cx = width / 2
                                    var cy = height / 2
                                    var r  = width / 2 - 4

                                    // Background disc
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r - 2, 0, Math.PI * 2)
                                    ctx.fillStyle = "#080E18"; ctx.fill()

                                    // Outer metallic ring
                                    var rg = ctx.createLinearGradient(0, 0, width, height)
                                    rg.addColorStop(0.0, "#2A3A50")
                                    rg.addColorStop(0.5, "#3A5070")
                                    rg.addColorStop(1.0, "#1A2535")
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r, 0, Math.PI * 2)
                                    ctx.strokeStyle = rg; ctx.lineWidth = 7; ctx.stroke()

                                    // Gauge: start 150° (8-o'clock), 240° clockwise sweep
                                    var startDeg = 150; var sweepDeg = 240
                                    var sRad = startDeg * Math.PI / 180
                                    var eRad = (startDeg + sweepDeg) * Math.PI / 180

                                    // Background track
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r - 14, sRad, eRad, false)
                                    ctx.strokeStyle = "#1A2535"; ctx.lineWidth = 13
                                    ctx.lineCap = "round"; ctx.stroke()

                                    // Blue fill (0 → 60 mph = first 120° of 240°)
                                    var fillRad = (startDeg + 120) * Math.PI / 180
                                    var ag = ctx.createLinearGradient(
                                        cx + Math.cos(sRad) * (r - 14),
                                        cy + Math.sin(sRad) * (r - 14),
                                        cx + Math.cos(fillRad) * (r - 14),
                                        cy + Math.sin(fillRad) * (r - 14))
                                    ag.addColorStop(0.0, "#103888"); ag.addColorStop(1.0, "#1A70FF")
                                    ctx.beginPath()
                                    ctx.arc(cx, cy, r - 14, sRad, fillRad, false)
                                    ctx.strokeStyle = ag; ctx.lineWidth = 13
                                    ctx.lineCap = "round"; ctx.stroke()

                                    // Ticks: 24 intervals = every 5 mph
                                    for (var i = 0; i <= 24; i++) {
                                        var a = (startDeg + sweepDeg * (i / 24)) * Math.PI / 180
                                        var major  = (i % 6 === 0)
                                        var medium = (i % 3 === 0)
                                        var oR = r - 8
                                        var iR = major ? r - 24 : (medium ? r - 18 : r - 14)
                                        ctx.beginPath()
                                        ctx.moveTo(cx + Math.cos(a) * oR, cy + Math.sin(a) * oR)
                                        ctx.lineTo(cx + Math.cos(a) * iR, cy + Math.sin(a) * iR)
                                        ctx.strokeStyle = major ? "#4A6888" : "#253545"
                                        ctx.lineWidth = major ? 2.5 : 1
                                        ctx.lineCap = "round"; ctx.stroke()
                                    }

                                    // Speed labels: 0, 30, 60, 90, 120
                                    ctx.textAlign = "center"; ctx.textBaseline = "middle"
                                    ctx.fillStyle = "#4A6888"; ctx.font = "bold 11px sans-serif"
                                    var lbls = [0, 30, 60, 90, 120]
                                    for (var j = 0; j < lbls.length; j++) {
                                        var la = (startDeg + sweepDeg * (lbls[j] / 120)) * Math.PI / 180
                                        ctx.fillText(String(lbls[j]),
                                                     cx + Math.cos(la) * (r - 32),
                                                     cy + Math.sin(la) * (r - 32))
                                    }

                                    // Needle at 60 mph (270° = 12 o'clock)
                                    var nRad = fillRad  // 60 mph angle
                                    var nLen = r - 22
                                    ctx.beginPath()
                                    ctx.moveTo(cx - Math.cos(nRad) * 8, cy - Math.sin(nRad) * 8)
                                    ctx.lineTo(cx + Math.cos(nRad) * nLen * 0.75,
                                               cy + Math.sin(nRad) * nLen * 0.75)
                                    ctx.strokeStyle = "#8AACC8"; ctx.lineWidth = 2
                                    ctx.lineCap = "round"; ctx.stroke()

                                    ctx.beginPath()
                                    ctx.moveTo(cx + Math.cos(nRad) * nLen * 0.75,
                                               cy + Math.sin(nRad) * nLen * 0.75)
                                    ctx.lineTo(cx + Math.cos(nRad) * nLen,
                                               cy + Math.sin(nRad) * nLen)
                                    ctx.strokeStyle = "#FF3040"; ctx.lineWidth = 2
                                    ctx.lineCap = "round"; ctx.stroke()

                                    // Center hub
                                    ctx.beginPath(); ctx.arc(cx, cy, 9, 0, Math.PI * 2)
                                    ctx.fillStyle = "#1A2A3A"; ctx.fill()
                                    ctx.strokeStyle = "#4A7AAA"; ctx.lineWidth = 2; ctx.stroke()
                                    ctx.beginPath(); ctx.arc(cx, cy, 4, 0, Math.PI * 2)
                                    ctx.fillStyle = "#6090C0"; ctx.fill()

                                    // Speed readout
                                    ctx.textAlign = "center"; ctx.textBaseline = "middle"
                                    ctx.font = "bold 30px sans-serif"; ctx.fillStyle = "#90BADC"
                                    ctx.fillText("60", cx, cy + 36)
                                    ctx.font = "10px sans-serif"; ctx.fillStyle = "#3A5060"
                                    ctx.fillText("MPH", cx, cy + 54)
                                }
                            }
                        }

                        // ── Right turn arrow ──────────────────────────────────
                        Item {
                            id: rightArrowItem
                            Layout.preferredWidth: 86
                            Layout.fillHeight: true

                            property bool blinkState: true

                            Column {
                                anchors.centerIn: parent; spacing: 6
                                Text {
                                    id: rightChevron
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "❯❯"; font.pixelSize: 40; color: root.clAmber
                                    opacity: hmi.rightArrowActive
                                             ? (rightArrowItem.blinkState ? 1.0 : 0.15)
                                             : 0.12
                                }
                                Text {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    text: "R"
                                    font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 3.0
                                    color: root.clDim
                                }
                            }

                            Timer {
                                interval: 500; repeat: true
                                running: hmi.rightArrowActive
                                onTriggered:      rightArrowItem.blinkState = !rightArrowItem.blinkState
                                onRunningChanged: if (!running) rightArrowItem.blinkState = true
                            }
                        }
                    }
                }
            }

            // ── Node health panel ─────────────────────────────────────────────
            Rectangle {
                Layout.preferredWidth: 192
                Layout.fillHeight: true
                color: root.clPanel

                Rectangle {
                    anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
                    width: 1; color: root.clBorderHi
                }

                Column {
                    anchors { fill: parent; margins: 18; topMargin: 22 }
                    spacing: 16

                    Text {
                        text: "NODE HEALTH"; color: root.clDimText
                        font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 2.2
                    }

                    // ETH
                    Item {
                        property bool blinkState: true
                        width: parent.width; implicitHeight: ethRow.implicitHeight
                        Row {
                            id: ethRow; spacing: 10
                            Rectangle {
                                id: ethDot; width: 9; height: 9; radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                color: hmi.ethUp ? root.clGreen : root.clRed
                                opacity: hmi.ethUp ? (parent.parent.blinkState ? 1.0 : 0.3) : 1.0
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: hmi.ethUp ? "ETH  UP" : "ETH  DOWN"
                                color: hmi.ethUp ? root.clText : "#6A3040"
                                font.pixelSize: 11; font.weight: Font.Medium; font.letterSpacing: 0.5
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                        Timer {
                            interval: 900; repeat: true; running: hmi.ethUp
                            onTriggered:      parent.blinkState = !parent.blinkState
                            onRunningChanged: if (!running) parent.blinkState = true
                        }
                    }

                    // SVC
                    Item {
                        property bool blinkState: true
                        width: parent.width; implicitHeight: svcRow.implicitHeight
                        Row {
                            id: svcRow; spacing: 10
                            Rectangle {
                                width: 9; height: 9; radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                color: hmi.svcUp ? root.clGreen : root.clRed
                                opacity: hmi.svcUp ? (parent.parent.blinkState ? 1.0 : 0.3) : 1.0
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: hmi.svcUp ? "SVC  UP" : "SVC  DOWN"
                                color: hmi.svcUp ? root.clText : "#6A3040"
                                font.pixelSize: 11; font.weight: Font.Medium; font.letterSpacing: 0.5
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                        Timer {
                            interval: 900; repeat: true; running: hmi.svcUp
                            onTriggered:      parent.blinkState = !parent.blinkState
                            onRunningChanged: if (!running) parent.blinkState = true
                        }
                    }

                    // CTRL
                    Item {
                        property bool blinkState: true
                        width: parent.width; implicitHeight: ctrlHRow.implicitHeight
                        Row {
                            id: ctrlHRow; spacing: 10
                            Rectangle {
                                width: 9; height: 9; radius: 5
                                anchors.verticalCenter: parent.verticalCenter
                                color: hmi.controllerOnline ? root.clGreen : root.clRed
                                opacity: hmi.controllerOnline
                                         ? (parent.parent.blinkState ? 1.0 : 0.3) : 1.0
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                text: hmi.controllerOnline ? "CTRL  ONLINE" : "CTRL  OFFLINE"
                                color: hmi.controllerOnline ? root.clText : "#6A3040"
                                font.pixelSize: 11; font.weight: Font.Medium; font.letterSpacing: 0.5
                                Behavior on color { ColorAnimation { duration: 250 } }
                            }
                        }
                        Timer {
                            interval: 900; repeat: true; running: hmi.controllerOnline
                            onTriggered:      parent.blinkState = !parent.blinkState
                            onRunningChanged: if (!running) parent.blinkState = true
                        }
                    }

                    Rectangle { width: parent.width; height: 1; color: root.clBorder }

                    Text {
                        readonly property bool allUp: hmi.ethUp && hmi.svcUp && hmi.controllerOnline
                        text:  allUp ? "OPERATIONAL" : "DEGRADED"
                        color: allUp ? root.clGreen : "#C07030"
                        font.pixelSize: 11; font.weight: Font.DemiBold; font.letterSpacing: 1.4
                        Behavior on color { ColorAnimation { duration: 300 } }
                    }

                    // FAULT row — shows count in red when any fault is active.
                    Row {
                        spacing: 6
                        Rectangle {
                            width: 9; height: 9; radius: 5
                            anchors.verticalCenter: parent.verticalCenter
                            color: hmi.faultPresent ? root.clRed : root.clDim
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "FAULT: " + hmi.activeFaultCount
                            color: hmi.faultPresent ? root.clRed : root.clDimText
                            font.pixelSize: 11; font.weight: Font.Medium; font.letterSpacing: 0.5
                            Behavior on color { ColorAnimation { duration: 250 } }
                        }
                    }

                    Item { height: 4 }

                    Rectangle {
                        width: parent.width; height: 32; radius: 6
                        color: reqArea.containsMouse ? "#1A2535" : "transparent"
                        border.color: root.clBorder; border.width: 1
                        Behavior on color { ColorAnimation { duration: 100 } }
                        Text {
                            anchors.centerIn: parent; text: "REQUEST"
                            color: root.clDimText
                            font.pixelSize: 10; font.weight: Font.Medium; font.letterSpacing: 1.4
                        }
                        MouseArea {
                            id: reqArea; anchors.fill: parent; hoverEnabled: true
                            onClicked: hmi.requestNodeHealth()
                        }
                    }
                }
            }
        }

        // ── Bottom button row ─────────────────────────────────────────────────
        Rectangle {
            Layout.fillWidth: true; height: 82
            color: root.clPanel
            Rectangle {
                anchors { top: parent.top; left: parent.left; right: parent.right }
                height: 1; color: root.clBorderHi
            }
            RowLayout {
                anchors { fill: parent; leftMargin: 20; rightMargin: 20;
                          topMargin: 8; bottomMargin: 8 }
                spacing: 12

                LampBtn { Layout.fillWidth: true; fn: 1; icon: "◄"; label: "LEFT IND"  }
                LampBtn { Layout.fillWidth: true; fn: 2; icon: "►"; label: "RIGHT IND" }
                LampBtn { Layout.fillWidth: true; fn: 3; icon: "⚠"; label: "HAZARD"    }
                LampBtn { Layout.fillWidth: true; fn: 4; icon: "●"; label: "PARK"      }
                LampBtn { Layout.fillWidth: true; fn: 5; icon: "☀"; label: "HEAD"      }
            }
        }
    }
}

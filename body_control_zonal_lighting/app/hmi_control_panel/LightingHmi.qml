import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    title: "Body Control Zonal Lighting — HMI"
    width: 940
    height: 480
    minimumWidth: 760
    minimumHeight: 420
    visible: true
    color: "#0d1117"

    // Mirror of domain::LampFunction enum values.
    readonly property int kLeftIndicator:  1
    readonly property int kRightIndicator: 2
    readonly property int kHazardLamp:     3
    readonly property int kParkLamp:       4
    readonly property int kHeadLamp:       5

    // Local lamp state cache.  Updated via Connections when lampStatusChanged fires
    // so that Q_INVOKABLE calls read the freshest ViewModel state.
    property var lampOutputOn: ({1: false, 2: false, 3: false, 4: false, 5: false})
    property var lampActive:   ({1: false, 2: false, 3: false, 4: false, 5: false})

    Component.onCompleted: refreshAllLamps()

    function refreshAllLamps() {
        var on  = {}
        var act = {}
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
            var on  = Object.assign({}, root.lampOutputOn)
            var act = Object.assign({}, root.lampActive)
            on[lampFunction]  = hmi.isLampOutputOn(lampFunction)
            act[lampFunction] = hmi.isLampCommandActive(lampFunction)
            root.lampOutputOn = on
            root.lampActive   = act
        }
    }

    // ── Inline components ──────────────────────────────────────────────────────

    component StatusDot : Rectangle {
        property color dotColor: "#64748b"
        width: 10; height: 10; radius: 5
        color: dotColor
        Behavior on color { ColorAnimation { duration: 200 } }
    }

    component LampButton : Rectangle {
        id: btn

        property int    lampFunc:    0
        property string labelText:   ""
        property string iconText:    ""
        property bool   isBlinker:   false
        property color  activeColor: "#f59e0b"
        property bool   isOutputOn:  false
        property bool   isActive:    false

        radius: 12
        color: (btn.isActive || btn.isOutputOn)
               ? Qt.darker(btn.activeColor, 3.8) : "#161b27"
        border.color: btn.isActive    ? btn.activeColor
                    : btn.isOutputOn  ? Qt.lighter(btn.activeColor, 1.3)
                    : "#252d3d"
        border.width: 2

        Behavior on color        { ColorAnimation { duration: 180 } }
        Behavior on border.color { ColorAnimation { duration: 180 } }

        // Overlay that blinks for indicators/hazard when commanded active.
        Rectangle {
            id: blinkOverlay
            anchors.fill: parent
            radius: parent.radius
            color: btn.activeColor
            opacity: 0.0
        }

        SequentialAnimation {
            id: blinkAnim
            loops: Animation.Infinite
            NumberAnimation {
                target: blinkOverlay; property: "opacity"
                to: 0.28; duration: 440; easing.type: Easing.InOutSine
            }
            NumberAnimation {
                target: blinkOverlay; property: "opacity"
                to: 0.0;  duration: 440; easing.type: Easing.InOutSine
            }
        }

        onIsActiveChanged: {
            if (btn.isBlinker && btn.isActive) {
                blinkAnim.start()
            } else {
                blinkAnim.stop()
                blinkOverlay.opacity = 0.0
            }
        }

        // Button content.
        Column {
            anchors.centerIn: parent
            spacing: 10

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: btn.iconText
                font.pixelSize: 34
                color: btn.isActive    ? btn.activeColor
                     : btn.isOutputOn  ? Qt.lighter(btn.activeColor, 1.6)
                     : "#2e3a50"
                Behavior on color { ColorAnimation { duration: 180 } }
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: btn.labelText
                color: (btn.isActive || btn.isOutputOn) ? "#c8d3e0" : "#3d4f68"
                font.pixelSize: 11
                font.bold: true
                font.letterSpacing: 1.5
                horizontalAlignment: Text.AlignHCenter
                Behavior on color { ColorAnimation { duration: 180 } }
            }

            // ON / OFF badge.
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 54; height: 20; radius: 10
                color: btn.isOutputOn
                       ? Qt.darker(btn.activeColor, 3.0) : "#0d1117"
                border.color: btn.isOutputOn ? btn.activeColor : "#252d3d"
                border.width: 1
                Behavior on color { ColorAnimation { duration: 180 } }

                Text {
                    anchors.centerIn: parent
                    text: btn.isOutputOn ? "ON" : "OFF"
                    color: btn.isOutputOn ? btn.activeColor : "#3d4f68"
                    font.pixelSize: 10; font.bold: true; font.letterSpacing: 1
                }
            }
        }

        // Click + hover.
        MouseArea {
            id: btnMouse
            anchors.fill: parent
            hoverEnabled: true
            onClicked: {
                if      (btn.lampFunc === 1) hmi.toggleLeftIndicator()
                else if (btn.lampFunc === 2) hmi.toggleRightIndicator()
                else if (btn.lampFunc === 3) hmi.toggleHazardLamp()
                else if (btn.lampFunc === 4) hmi.toggleParkLamp()
                else if (btn.lampFunc === 5) hmi.toggleHeadLamp()
            }

            Rectangle {
                anchors.fill: parent; radius: parent.parent.radius
                color: "#ffffff"
                opacity: btnMouse.containsMouse ? 0.04 : 0.0
                Behavior on opacity { NumberAnimation { duration: 100 } }
            }
        }
    }

    // ── Main layout ────────────────────────────────────────────────────────────

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 12

        // Title / controller status bar.
        Rectangle {
            Layout.fillWidth: true
            height: 46
            radius: 8
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#1a2236" }
                GradientStop { position: 1.0; color: "#0d1117" }
            }
            border.color: "#252d3d"; border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18; anchors.rightMargin: 18

                Text {
                    text: "BODY CONTROL ZONAL LIGHTING"
                    color: "#7c90aa"
                    font.pixelSize: 14; font.letterSpacing: 2.5; font.bold: true
                }

                Item { Layout.fillWidth: true }

                Row {
                    spacing: 8
                    StatusDot {
                        anchors.verticalCenter: parent.verticalCenter
                        dotColor: hmi.controllerAvailable ? "#22c55e" : "#ef4444"
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: hmi.controllerAvailable
                              ? "CONTROLLER: ONLINE" : "CONTROLLER: OFFLINE"
                        color: hmi.controllerAvailable ? "#86efac" : "#fca5a5"
                        font.pixelSize: 12; font.letterSpacing: 1
                    }
                }
            }
        }

        // Five lamp buttons.
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            LampButton {
                Layout.fillWidth: true; Layout.fillHeight: true
                lampFunc: 1; labelText: "LEFT\nINDICATOR"; iconText: "◁"
                isBlinker: true; activeColor: "#f59e0b"
                isOutputOn: root.lampOutputOn[1] === true
                isActive:   root.lampActive[1]   === true
            }
            LampButton {
                Layout.fillWidth: true; Layout.fillHeight: true
                lampFunc: 2; labelText: "RIGHT\nINDICATOR"; iconText: "▷"
                isBlinker: true; activeColor: "#f59e0b"
                isOutputOn: root.lampOutputOn[2] === true
                isActive:   root.lampActive[2]   === true
            }
            LampButton {
                Layout.fillWidth: true; Layout.fillHeight: true
                lampFunc: 3; labelText: "HAZARD"; iconText: "⚠"
                isBlinker: true; activeColor: "#ef4444"
                isOutputOn: root.lampOutputOn[3] === true
                isActive:   root.lampActive[3]   === true
            }
            LampButton {
                Layout.fillWidth: true; Layout.fillHeight: true
                lampFunc: 4; labelText: "PARK\nLAMP"; iconText: "◎"
                isBlinker: false; activeColor: "#3b82f6"
                isOutputOn: root.lampOutputOn[4] === true
                isActive:   root.lampActive[4]   === true
            }
            LampButton {
                Layout.fillWidth: true; Layout.fillHeight: true
                lampFunc: 5; labelText: "HEAD\nLAMP"; iconText: "☀"
                isBlinker: false; activeColor: "#facc15"
                isOutputOn: root.lampOutputOn[5] === true
                isActive:   root.lampActive[5]   === true
            }
        }

        // Node health bar.
        Rectangle {
            Layout.fillWidth: true
            height: 56
            radius: 8
            color: "#111827"
            border.color: "#252d3d"; border.width: 1

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20; anchors.rightMargin: 20
                spacing: 20

                Text {
                    text: "NODE HEALTH"
                    color: "#4a5568"
                    font.pixelSize: 11; font.letterSpacing: 1.5; font.bold: true
                }

                Row {
                    spacing: 6
                    StatusDot {
                        anchors.verticalCenter: parent.verticalCenter
                        dotColor: hmi.ethUp ? "#22c55e" : "#ef4444"
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "ETH: " + (hmi.ethUp ? "UP" : "DOWN")
                        color: hmi.ethUp ? "#86efac" : "#fca5a5"
                        font.pixelSize: 13
                    }
                }

                Row {
                    spacing: 6
                    StatusDot {
                        anchors.verticalCenter: parent.verticalCenter
                        dotColor: hmi.svcUp ? "#22c55e" : "#ef4444"
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "SVC: " + (hmi.svcUp ? "UP" : "DOWN")
                        color: hmi.svcUp ? "#86efac" : "#fca5a5"
                        font.pixelSize: 13
                    }
                }

                Row {
                    spacing: 6
                    StatusDot {
                        anchors.verticalCenter: parent.verticalCenter
                        dotColor: hmi.faultPresent ? "#ef4444" : "#22c55e"
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: hmi.faultPresent
                              ? "FAULT: YES (" + hmi.faultCount + ")"
                              : "FAULT: NONE"
                        color: hmi.faultPresent ? "#fca5a5" : "#86efac"
                        font.pixelSize: 13
                    }
                }

                Item { Layout.fillWidth: true }

                Text {
                    readonly property var stateNames:
                        ["UNKNOWN", "OPERATIONAL", "DEGRADED", "FAULTED", "UNAVAILABLE"]
                    text: stateNames[Math.min(hmi.healthState, 4)]
                    color: hmi.healthState === 1 ? "#22c55e"
                         : hmi.healthState === 2 ? "#f59e0b"
                         : hmi.healthState >= 3  ? "#ef4444"
                         : "#4a5568"
                    font.pixelSize: 12; font.bold: true; font.letterSpacing: 1.5
                }

                Rectangle {
                    width: 96; height: 30; radius: 6
                    color: reqArea.containsMouse ? "#1e293b" : "#0d1117"
                    border.color: "#374151"; border.width: 1
                    Behavior on color { ColorAnimation { duration: 100 } }

                    Text {
                        anchors.centerIn: parent
                        text: "REQUEST"
                        color: "#6b7280"; font.pixelSize: 11; font.letterSpacing: 1
                    }
                    MouseArea {
                        id: reqArea; anchors.fill: parent; hoverEnabled: true
                        onClicked: hmi.requestNodeHealth()
                    }
                }
            }
        }
    }
}

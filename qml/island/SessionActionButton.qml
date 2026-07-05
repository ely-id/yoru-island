pragma ComponentBehavior: Bound

import QtQuick
import Quickshell
import IslandBackend

Item {
    id: root

    property string label: ""
    property string icon: ""
    property var command: []
    property bool needsConfirm: false
    property bool armed: false
    property string iconFontFamily: ""
    property string textFontFamily: ""

    signal buttonPressed()
    signal actionExecuted()

    width: 56
    height: 78

    Timer {
        id: confirmResetTimer
        interval: 3000
        repeat: false
        onTriggered: root.armed = false
    }

    function trigger() {
        if (root.needsConfirm && !root.armed) {
            root.armed = true;
            confirmResetTimer.restart();
            return;
        }
        root.armed = false;
        confirmResetTimer.stop();
        Quickshell.execDetached(root.command);
        root.actionExecuted();
    }

    Rectangle {
        id: tile
        width: 56
        height: 56
        radius: 18
        anchors.horizontalCenter: parent.horizontalCenter
        color: root.armed ? StyleTokens.danger
             : buttonArea.pressed ? StyleTokens.connectivityCard
             : buttonArea.containsMouse ? StyleTokens.track
             : StyleTokens.module
        scale: buttonArea.pressed ? 0.92 : 1.0

        Behavior on scale {
            NumberAnimation { duration: 100 }
        }
        Behavior on color {
            ColorAnimation { duration: 120 }
        }

        Text {
            anchors.centerIn: parent
            text: root.icon
            color: StyleTokens.textPrimaryBright
            font.family: root.iconFontFamily
            font.pixelSize: 26
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    Text {
        anchors.top: tile.bottom
        anchors.topMargin: 5
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.armed ? "Confirm?" : root.label
        color: root.armed ? StyleTokens.danger : StyleTokens.textSecondary
        font.family: root.textFontFamily
        font.pixelSize: 10
        horizontalAlignment: Text.AlignHCenter
    }

    MouseArea {
        id: buttonArea
        anchors.fill: parent
        hoverEnabled: true
        preventStealing: true

        onPressed: function(mouse) {
            root.buttonPressed();
            mouse.accepted = true;
        }
        onClicked: root.trigger()
    }
}

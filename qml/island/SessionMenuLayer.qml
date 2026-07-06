pragma ComponentBehavior: Bound

import QtQuick
import IslandBackend

Item {
    id: root

    property bool showCondition: false
    property string iconFontFamily: ""
    property string textFontFamily: ""

    signal controlPressed()
    signal closeRequested()

    opacity: showCondition ? 1 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: 180
            easing.type: Easing.InOutQuad
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: root.closeRequested()
    }

    Row {
        anchors.centerIn: parent
        spacing: 12

        SessionActionButton {
            label: "Lock"
            icon: "󰌾"  // nf-md-lock U+F033E
            command: ["sh", "-c", "pidof hyprlock || hyprlock"]
            iconFontFamily: root.iconFontFamily
            textFontFamily: root.textFontFamily
            onButtonPressed: root.controlPressed()
            onActionExecuted: root.closeRequested()
        }

        SessionActionButton {
            label: "Suspend"
            icon: "󰤄"  // nf-md-power_sleep U+F0904
            command: ["systemctl", "suspend"]
            iconFontFamily: root.iconFontFamily
            textFontFamily: root.textFontFamily
            onButtonPressed: root.controlPressed()
            onActionExecuted: root.closeRequested()
        }

        SessionActionButton {
            label: "Logout"
            icon: "󰍃"  // nf-md-logout U+F0343
            needsConfirm: true
            command: ["hyprshutdown"]
            iconFontFamily: root.iconFontFamily
            textFontFamily: root.textFontFamily
            onButtonPressed: root.controlPressed()
            onActionExecuted: root.closeRequested()
        }

        SessionActionButton {
            label: "Reboot"
            icon: "󰜉"  // nf-md-restart U+F0709
            needsConfirm: true
            command: ["systemctl", "reboot"]
            iconFontFamily: root.iconFontFamily
            textFontFamily: root.textFontFamily
            onButtonPressed: root.controlPressed()
            onActionExecuted: root.closeRequested()
        }

        SessionActionButton {
            label: "Shutdown"
            icon: "󰐥"  // nf-md-power U+F0425
            needsConfirm: true
            command: ["systemctl", "poweroff"]
            iconFontFamily: root.iconFontFamily
            textFontFamily: root.textFontFamily
            onButtonPressed: root.controlPressed()
            onActionExecuted: root.closeRequested()
        }
    }
}

import QtQuick
import QtQuick.Controls
import TideIsland 1.0

Rectangle {
    id: root

    property int revision: 0

    color: "transparent"
    radius: 10
    border.width: 2
    border.color: Theme.splitLineColor
    implicitHeight: tlpColumn.implicitHeight + 36

    function textValue(key, fallback) {
        return String(ConfigStore.value(key, fallback))
    }

    function permissionMode() {
        revision
        const mode = textValue("tlpPermissionMode", "skip").trim()
        return mode.length > 0 ? mode : "skip"
    }

    function tlpEnabled() {
        const mode = permissionMode()
        return mode !== "skip"
    }

    function passwordValue() {
        revision
        return textValue("tlpSudoPassword", "")
    }

    function saveEnabled(enabled) {
        ConfigStore.setValue("tlpPermissionMode", enabled ? "password" : "skip")
        ConfigStore.save()
        revision += 1
    }

    function savePassword(value) {
        ConfigStore.setValue("tlpSudoPassword", String(value))
        if (tlpEnabled())
            ConfigStore.setValue("tlpPermissionMode", "password")
        ConfigStore.save()
        revision += 1
    }

    Column {
        id: tlpColumn

        anchors.top: parent.top
        anchors.topMargin: 18
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.right: parent.right
        anchors.rightMargin: 18
        spacing: 16

        ToggleRow {
            title: "Enable TLP"
            description: "Show TLP power profile controls in Control Center"
            width: parent.width
        }

        SplitLine { width: parent.width }

        PasswordRow {
            title: "Sudo Password"
            description: "Used when switching TLP power profiles"
            width: parent.width
        }
    }

    component SplitLine: Rectangle {
        height: 2
        color: Theme.splitLineColor
    }

    component ToggleRow: Item {
        id: row

        property string title: ""
        property string description: ""

        height: 49

        Text {
            id: rowTitle

            text: row.title
            anchors.left: parent.left
            anchors.top: parent.top
            color: Theme.textColor
            font.family: Theme.textFontFamily
            font.pixelSize: 18
        }

        Text {
            text: row.description
            anchors.left: rowTitle.left
            anchors.top: rowTitle.bottom
            anchors.topMargin: 5
            color: Theme.subtleTextColor
            font.family: Theme.textFontFamily
            font.pixelSize: 14
        }

        StyledSwitch {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            checked: root.tlpEnabled()

            onToggled: function(checked) {
                root.saveEnabled(checked)
            }
        }
    }

    component PasswordRow: Item {
        id: row

        property string title: ""
        property string description: ""

        height: 49

        Text {
            id: rowTitle

            text: row.title
            anchors.left: parent.left
            anchors.top: parent.top
            color: Theme.textColor
            font.family: Theme.textFontFamily
            font.pixelSize: 18
        }

        Text {
            text: row.description
            anchors.left: rowTitle.left
            anchors.top: rowTitle.bottom
            anchors.topMargin: 5
            color: Theme.subtleTextColor
            font.family: Theme.textFontFamily
            font.pixelSize: 14
        }

        Rectangle {
            id: passwordBox

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(180, Math.min(300, parent.width / 3))
            height: 36
            radius: 8
            color: root.tlpEnabled() ? Theme.inputBgColor : "#d6d0c8"
            border.width: 2
            border.color: passwordField.activeFocus ? Theme.focusBorderColor : (root.tlpEnabled() ? Theme.inputBorderColor : "#c7beb5")

            Behavior on color {
                ColorAnimation { duration: Theme.animationDuration }
            }

            Behavior on border.color {
                ColorAnimation { duration: Theme.animationDuration }
            }

            TextField {
                id: passwordField

                anchors.fill: parent
                enabled: root.tlpEnabled()
                background: null
                echoMode: TextInput.Password
                color: root.tlpEnabled() ? Theme.textColor : Theme.subtleTextColor
                placeholderText: "Password"
                placeholderTextColor: Theme.subtleTextColor
                selectionColor: Theme.selectedColor
                selectedTextColor: Theme.buttonTextColor
                inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText
                leftPadding: 10
                rightPadding: 10
                verticalAlignment: TextInput.AlignVCenter
                font.family: Theme.textFontFamily
                font.pixelSize: 14

                Component.onCompleted: text = root.passwordValue()
                onAccepted: root.savePassword(text)
                onEditingFinished: root.savePassword(text)
            }
        }
    }

    component StyledSwitch: Item {
        id: control

        signal toggled(bool checked)

        property bool checked: false

        width: 48
        height: 26

        Rectangle {
            id: track

            anchors.verticalCenter: parent.verticalCenter
            width: 48
            height: 24
            radius: 12
            color: control.checked ? Theme.selectedColor : Qt.rgba(100 / 255, 116 / 255, 139 / 255, 0.377)

            Behavior on color {
                ColorAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }
        }

        Rectangle {
            id: knob

            width: 26
            height: 26
            radius: 13
            x: control.checked ? 22 : 0
            y: 0
            color: "white"
            border.width: 1
            border.color: control.checked ? Theme.selectedColor : Qt.rgba(100 / 255, 116 / 255, 139 / 255, 0.527)

            Behavior on x {
                NumberAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }

            Behavior on border.color {
                ColorAnimation { duration: 300; easing.type: Easing.InOutQuad }
            }
        }

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onClicked: control.toggled(!control.checked)
        }
    }
}

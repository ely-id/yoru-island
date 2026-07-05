import TideIsland 1.0
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Rectangle {
    id: root

    readonly property var transitionTypes: [
        "none", "simple", "fade", "left", "right", "top", "bottom",
        "wipe", "wave", "grow", "center", "any", "outer", "random"
    ]

    color: "transparent"
    radius: 10
    border.width: 2
    border.color: Theme.splitLineColor
    implicitHeight: wallpaperColumn.implicitHeight + 36

    function textValue(key, fallback) {
        return String(ConfigStore.value(key, fallback))
    }

    function boolValue(key, fallback) {
        const value = ConfigStore.value(key, fallback)
        return value === true || value === "true"
    }

    function localPath(value) {
        if (value === undefined || value === null)
            return ""
        if (value.toLocalFile)
            return value.toLocalFile()

        const text = String(value)
        return text.startsWith("file://") ? decodeURIComponent(text.substring(7)) : text
    }

    function folderUrl(path) {
        const text = String(path || "")
        if (text.length === 0)
            return StandardPaths.writableLocation(StandardPaths.HomeLocation)
        return text.startsWith("file://") ? text : "file://" + encodeURI(text)
    }

    function parentFolderUrl(path) {
        const text = localPath(path)
        const slashIndex = text.lastIndexOf("/")
        if (slashIndex <= 0)
            return StandardPaths.writableLocation(StandardPaths.HomeLocation)
        return folderUrl(text.substring(0, slashIndex))
    }

    function saveText(key, value) {
        ConfigStore.setValue(key, String(value))
        ConfigStore.save()
    }

    function saveBool(key, value) {
        ConfigStore.setValue(key, !!value)
        ConfigStore.save()
    }

    Column {
        id: wallpaperColumn

        anchors.top: parent.top
        anchors.topMargin: 18
        anchors.left: parent.left
        anchors.leftMargin: 18
        anchors.right: parent.right
        anchors.rightMargin: 18
        spacing: 16

        PathRow {
            title: "Wallpaper Path"
            description: "Target file used by awww and workspace overview"
            keyName: "wallpaperPath"
            fallbackText: ""
            directoryMode: false
            width: parent.width
        }

        SplitLine { width: parent.width }

        PathRow {
            title: "Wallpaper Library"
            description: "Folder scanned by the wallpaper picker"
            keyName: "wallpaperLibraryPath"
            fallbackText: ""
            directoryMode: true
            width: parent.width
        }

        SplitLine { width: parent.width }

        ToggleRow {
            title: "Pywal"
            description: "Run wal -i after awww applies a wallpaper"
            keyName: "wallpaperPywalEnabled"
            fallbackValue: false
            width: parent.width
        }

        SplitLine { width: parent.width }

        Item {
            width: parent.width
            height: transitionColumn.implicitHeight

            Column {
                id: transitionColumn

                width: parent.width
                spacing: 14

                TransitionRow {
                    width: parent.width
                }
            }
        }
    }

    FileDialog {
        id: wallpaperFileDialog

        title: "Choose wallpaper target"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.gif *.avif *.bmp)", "All files (*)"]

        onAccepted: {
            if (pathRowForDialog)
                pathRowForDialog.setPath(root.localPath(selectedFile))
        }
    }

    FolderDialog {
        id: wallpaperFolderDialog

        title: "Choose wallpaper library"

        onAccepted: {
            if (pathRowForDialog)
                pathRowForDialog.setPath(root.localPath(selectedFolder))
        }
    }

    property var pathRowForDialog: null

    component SplitLine: Rectangle {
        height: 2
        color: Theme.splitLineColor
    }

    component PathRow: Item {
        id: row

        property string title: ""
        property string description: ""
        property string keyName: ""
        property string fallbackText: ""
        property bool directoryMode: false

        height: 49

        Text {
            id: rowTitle
            text: row.title
            font.family: Theme.textFontFamily
            font.pixelSize: 18
            color: Theme.textColor
            anchors.left: parent.left
            anchors.top: parent.top
        }

        Text {
            text: row.description
            font.family: Theme.textFontFamily
            font.pixelSize: 14
            color: Theme.subtleTextColor
            anchors.left: rowTitle.left
            anchors.top: rowTitle.bottom
            anchors.topMargin: 5
        }

        ConfigTextField {
            id: field
            anchors.right: browseButton.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            width: Math.max(180, Math.min(360, (parent.width - browseButton.width - 34) / 2))
            height: 36
            placeholderText: row.fallbackText
            textPixelSize: 13

            Component.onCompleted: text = root.textValue(row.keyName, row.fallbackText)
            onAccepted: row.commit()
            onEditingFinished: row.commit()
        }

        ButtonLike {
            id: browseButton

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "Browse"

            onClicked: {
                root.pathRowForDialog = row
                if (row.directoryMode) {
                    wallpaperFolderDialog.currentFolder = root.folderUrl(field.text)
                    wallpaperFolderDialog.open()
                } else {
                    wallpaperFileDialog.currentFolder = root.parentFolderUrl(field.text)
                    wallpaperFileDialog.open()
                }
            }
        }

        function setPath(path) {
            field.text = path
            commit()
        }

        function commit() {
            root.saveText(row.keyName, field.text)
        }
    }

    component ToggleRow: Item {
        id: row

        property string title: ""
        property string description: ""
        property string keyName: ""
        property bool fallbackValue: false

        height: 49

        Text {
            id: rowTitle
            text: row.title
            font.family: Theme.textFontFamily
            font.pixelSize: 18
            color: Theme.textColor
            anchors.left: parent.left
            anchors.top: parent.top
        }

        Text {
            text: row.description
            font.family: Theme.textFontFamily
            font.pixelSize: 14
            color: Theme.subtleTextColor
            anchors.left: rowTitle.left
            anchors.top: rowTitle.bottom
            anchors.topMargin: 5
        }

        StyledSwitch {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            checked: root.boolValue(row.keyName, row.fallbackValue)

            onToggled: root.saveBool(row.keyName, checked)
        }
    }

    component ButtonLike: Rectangle {
        id: button

        signal clicked

        property string text: ""

        width: 78
        height: 36
        radius: 8
        color: mouseArea.pressed ? Theme.buttonHoverColor : mouseArea.containsMouse ? Theme.buttonHoverColor : Theme.buttonColor

        Text {
            anchors.centerIn: parent
            text: button.text
            color: Theme.buttonTextColor
            font.family: Theme.textFontFamily
            font.pixelSize: 13
            font.weight: Font.DemiBold
        }

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: button.clicked()
        }
    }

    component TransitionRow: Item {
        id: row

        height: 49

        Text {
            id: transitionTitle
            text: "Transition"
            font.family: Theme.textFontFamily
            font.pixelSize: 18
            color: Theme.textColor
            anchors.left: parent.left
            anchors.top: parent.top
        }

        Text {
            text: "awww wallpaper switch animation"
            font.family: Theme.textFontFamily
            font.pixelSize: 14
            color: Theme.subtleTextColor
            anchors.left: transitionTitle.left
            anchors.top: transitionTitle.bottom
            anchors.topMargin: 5
        }

        ComboBox {
            id: transitionTypeBox

            property bool ready: false

            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 170
            height: 36
            model: root.transitionTypes

            background: Rectangle {
                radius: 8
                color: transitionTypeBox.pressed ? Theme.accentSoftColor : Theme.inputBgColor
                border.width: 2
                border.color: transitionTypeBox.activeFocus ? Theme.focusBorderColor : Theme.inputBorderColor

                Behavior on color {
                    ColorAnimation { duration: Theme.animationDuration }
                }

                Behavior on border.color {
                    ColorAnimation { duration: Theme.animationDuration }
                }
            }

            contentItem: Text {
                leftPadding: 12
                rightPadding: 34
                text: transitionTypeBox.displayText
                color: Theme.textColor
                font.family: Theme.textFontFamily
                font.pixelSize: 14
                font.weight: Font.DemiBold
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            indicator: Text {
                x: transitionTypeBox.width - width - 12
                y: (transitionTypeBox.height - height) / 2
                text: "v"
                color: Theme.selectedColor
                font.family: Theme.textFontFamily
                font.pixelSize: 13
                font.weight: Font.Bold
            }

            popup: Popup {
                y: transitionTypeBox.height + 6
                width: transitionTypeBox.width
                implicitHeight: Math.min(contentItem.implicitHeight + 8, 260)
                padding: 4

                background: Rectangle {
                    radius: 8
                    color: Theme.cardBgColor
                    border.width: 1
                    border.color: Theme.cardBorderColor
                }

                contentItem: ListView {
                    clip: true
                    implicitHeight: contentHeight
                    model: transitionTypeBox.popup.visible ? transitionTypeBox.delegateModel : null
                    currentIndex: transitionTypeBox.highlightedIndex
                }
            }

            delegate: ItemDelegate {
                width: transitionTypeBox.width - 8
                height: 34
                highlighted: transitionTypeBox.highlightedIndex === index

                background: Rectangle {
                    radius: 6
                    color: highlighted ? Theme.accentSoftColor : "transparent"
                }

                contentItem: Text {
                    text: modelData
                    color: highlighted || transitionTypeBox.currentIndex === index ? Theme.selectedColor : Theme.textColor
                    font.family: Theme.textFontFamily
                    font.pixelSize: 13
                    font.weight: transitionTypeBox.currentIndex === index ? Font.DemiBold : Font.Normal
                    verticalAlignment: Text.AlignVCenter
                    leftPadding: 8
                }
            }

            Component.onCompleted: {
                const current = root.textValue("wallpaperTransitionType", "center")
                const index = root.transitionTypes.indexOf(current)
                currentIndex = index >= 0 ? index : root.transitionTypes.indexOf("center")
                ready = true
            }

            onActivated: {
                if (ready)
                    root.saveText("wallpaperTransitionType", currentText)
            }
        }
    }

    component StyledSwitch: Item {
        id: control

        signal toggled

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
            id: mouseArea

            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor

            onClicked: {
                control.checked = !control.checked
                control.toggled()
            }
        }
    }
}

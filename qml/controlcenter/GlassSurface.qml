import QtQuick
import IslandBackend

Item {
    id: root

    property real radius: 20
    property bool hovered: false
    property bool pressed: false
    readonly property real innerRadius: Math.max(0, radius - 1)

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: root.hovered ? "#484a4f" : "#3b3d42"
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: root.innerRadius
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: root.pressed ? "#202226" : (root.hovered ? "#303236" : "#292b2f")
            }
            GradientStop {
                position: 0.58
                color: root.pressed ? "#191b1f" : (root.hovered ? "#26282c" : "#222428")
            }
            GradientStop {
                position: 1.0
                color: root.pressed ? "#14161a" : "#1d1f23"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: root.innerRadius
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: root.hovered ? "#0bffffff" : "#06ffffff"
            }
            GradientStop {
                position: 0.06
                color: root.hovered ? "#04ffffff" : "#02ffffff"
            }
            GradientStop {
                position: 0.13
                color: StyleTokens.clearBlack
            }
            GradientStop {
                position: 1.0
                color: StyleTokens.clearBlack
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: root.innerRadius
        color: StyleTokens.transparent
        border.width: 1
        border.color: root.hovered ? "#0fffffff" : "#08ffffff"
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 1
        height: Math.min(8, parent.height * 0.16)
        radius: root.innerRadius
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: StyleTokens.clearBlack
            }
            GradientStop {
                position: 1.0
                color: "#16000000"
            }
        }
    }
}

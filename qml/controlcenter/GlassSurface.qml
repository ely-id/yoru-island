import QtQuick
import IslandBackend

Item {
    id: root

    property real radius: 20
    property bool hovered: false
    property bool pressed: false

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: root.pressed ? "#303238" : (root.hovered ? "#2d3035" : "#282b30")
            }
            GradientStop {
                position: 0.48
                color: root.pressed ? "#202328" : (root.hovered ? "#22262a" : "#1f2226")
            }
            GradientStop {
                position: 1.0
                color: root.pressed ? "#151719" : "#121416"
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        color: StyleTokens.transparent
        border.width: 1
        border.color: root.hovered ? "#26ffffff" : "#18ffffff"
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: "#0dffffff"
            }
            GradientStop {
                position: 0.28
                color: "#02ffffff"
            }
            GradientStop {
                position: 1.0
                color: StyleTokens.clearBlack
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop {
                position: 0.0
                color: "#03ffffff"
            }
            GradientStop {
                position: 0.42
                color: "#01ffffff"
            }
            GradientStop {
                position: 1.0
                color: StyleTokens.clearBlack
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        radius: root.radius
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: StyleTokens.clearBlack
            }
            GradientStop {
                position: 0.68
                color: StyleTokens.clearBlack
            }
            GradientStop {
                position: 1.0
                color: "#14000000"
            }
        }
    }
}

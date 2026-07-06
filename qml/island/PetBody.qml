pragma ComponentBehavior: Bound

import QtQuick

// Clawd procedural — marionete de primitivas.
// Canais animáveis: bounce, danceBounce, squash, lean, eyeOpen, eyeLookX, armLift, walkCycle.
// Os loops no fim do arquivo escrevem nos canais; a figura só lê.
Item {
    id: root

    property string mood: "idle"
    property real cavaLevel: 0

    readonly property real u: height / 28

    property real bounce: 0
    property real danceBounce: 0
    property real squash: 1
    property real lean: 0
    property real eyeOpen: 1
    property real eyeLookX: 0
    property real armLift: 0
    property real walkCycle: 0

    readonly property bool moving: mood === "walk" || mood === "run" || mood === "ball"

    implicitWidth: 26 * u

    onMoodChanged: {
        bounce = 0;
        lean = mood === "bubbles" ? -4 : 0;
        squash = 1;
        armLift = 0;
        eyeLookX = 0;
        eyeOpen = mood === "sleeping" ? 0.12 : (mood === "sleepy" ? 0.45 : 1);
    }

    // ---------- figura ----------
    Item {
        id: figure
        width: 20 * root.u
        height: 22 * root.u
        anchors.horizontalCenter: parent.horizontalCenter
        y: root.height - height + root.bounce + root.danceBounce

        transform: [
            Scale {
                origin.x: figure.width / 2
                origin.y: figure.height
                xScale: 1 + (1 - root.squash) * 0.6
                yScale: root.squash
            },
            Rotation {
                origin.x: figure.width / 2
                origin.y: figure.height
                angle: root.lean
            }
        ]

        // pernas (4 tocos; ciclo de passos quando em movimento)
        Repeater {
            model: 4
            delegate: Rectangle {
                required property int index
                width: 2.8 * root.u
                height: 4.5 * root.u
                radius: root.u
                color: "#FC7E03"
                x: (2.4 + index * 4.4) * root.u
                y: figure.height - height
                    - (root.moving
                        ? Math.max(0, Math.sin((root.walkCycle + index * 0.25) * Math.PI * 2)) * 1.6 * root.u
                        : 0)
            }
        }

        // braços-toco
        Rectangle {
            width: 3 * root.u
            height: 5 * root.u
            radius: root.u
            color: "#FC7E03"
            x: -2.2 * root.u
            y: 8 * root.u
        }
        Rectangle {
            id: armR
            width: 3 * root.u
            height: 5 * root.u
            radius: root.u
            color: "#D96A02"
            x: figure.width - 0.8 * root.u
            y: 8 * root.u
            transform: Rotation {
                origin.x: armR.width / 2
                origin.y: 0.8 * root.u
                angle: root.armLift
            }
        }

        // torso
        Rectangle {
            id: torso
            width: figure.width
            height: 17 * root.u
            y: 1.5 * root.u
            radius: 3 * root.u
            color: "#FC7E03"
            clip: true

            Rectangle {
                width: 2.2 * root.u
                height: parent.height
                anchors.right: parent.right
                radius: parent.radius
                color: "#D96A02"
            }

            Rectangle {
                id: eyeL
                width: 2.6 * root.u
                height: Math.max(0.5 * root.u, 3.6 * root.u * root.eyeOpen)
                radius: 0.5 * root.u
                color: "#151515"
                x: (5.2 + root.eyeLookX) * root.u
                y: 4.5 * root.u + (3.6 * root.u - height) / 2
            }
            Rectangle {
                width: 2.6 * root.u
                height: eyeL.height
                radius: 0.5 * root.u
                color: "#151515"
                x: (12.2 + root.eyeLookX) * root.u
                y: eyeL.y
            }

            // lágrima (sad)
            Rectangle {
                id: tear
                visible: root.mood === "sad"
                width: 1.4 * root.u
                height: 2 * root.u
                radius: 0.7 * root.u
                color: "#6F9FD8"
                x: eyeL.x + 0.4 * root.u
                y: eyeL.y + 4 * root.u

                SequentialAnimation {
                    running: tear.visible
                    loops: Animation.Infinite
                    PropertyAction { target: tear; property: "y"; value: eyeL.y + 4 * root.u }
                    PropertyAction { target: tear; property: "opacity"; value: 0.95 }
                    ParallelAnimation {
                        NumberAnimation { target: tear; property: "y"; to: eyeL.y + 10 * root.u; duration: 950; easing.type: Easing.InQuad }
                        NumberAnimation { target: tear; property: "opacity"; to: 0; duration: 950 }
                    }
                    PauseAnimation { duration: 1500 }
                }
            }
        }

        // fones (dance)
        Item {
            opacity: root.mood === "dance" ? 1 : 0
            visible: opacity > 0

            Behavior on opacity {
                NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
            }

            Rectangle {
                x: -0.5 * root.u
                y: -0.4 * root.u
                width: figure.width + root.u
                height: 1.6 * root.u
                radius: 0.8 * root.u
                color: "#F4F6FA"
            }
            Rectangle {
                x: -1.6 * root.u
                y: 3.2 * root.u
                width: 2.6 * root.u
                height: 5 * root.u
                radius: root.u
                color: "#2B4C9B"
            }
            Rectangle {
                x: figure.width - root.u
                y: 3.2 * root.u
                width: 2.6 * root.u
                height: 5 * root.u
                radius: root.u
                color: "#2B4C9B"
            }
        }

        // lâmpada (excited)
        Item {
            id: bulb
            visible: scale > 0.01
            scale: root.mood === "excited" ? 1 : 0
            x: figure.width - 2 * root.u
            y: -6.5 * root.u

            Behavior on scale {
                NumberAnimation { duration: 240; easing.type: Easing.OutBack }
            }

            Rectangle {
                width: 7 * root.u
                height: 7 * root.u
                radius: width / 2
                x: -1.25 * root.u
                y: -1.25 * root.u
                color: "#33FFD84D"
            }
            Rectangle {
                width: 4.5 * root.u
                height: 4.5 * root.u
                radius: width / 2
                color: "#FFD84D"
            }
            Rectangle {
                width: 2 * root.u
                height: 1.3 * root.u
                x: 1.25 * root.u
                y: 4.4 * root.u
                radius: 0.4 * root.u
                color: "#C9CDD6"
            }
        }
    }

    // zZz (sleeping)
    Item {
        id: zzz
        visible: root.mood === "sleeping"
        x: parent.width / 2 + 7 * root.u

        Repeater {
            model: 3
            delegate: Text {
                required property int index
                text: "z"
                color: "#8E8E93"
                font.pixelSize: (2.2 + index * 0.9) * root.u
                font.bold: true
                x: index * 2.2 * root.u
                y: -index * 2.4 * root.u
            }
        }

        SequentialAnimation {
            running: zzz.visible
            loops: Animation.Infinite
            ParallelAnimation {
                NumberAnimation { target: zzz; property: "y"; from: 9 * root.u; to: 4 * root.u; duration: 2400; easing.type: Easing.OutQuad }
                SequentialAnimation {
                    NumberAnimation { target: zzz; property: "opacity"; from: 0; to: 1; duration: 700 }
                    PauseAnimation { duration: 900 }
                    NumberAnimation { target: zzz; property: "opacity"; to: 0; duration: 800 }
                }
            }
        }
    }

    // ---------- loops de humor ----------

    // respiração normal
    SequentialAnimation {
        running: ["idle", "bubbles", "quirk", "sad", "wave", "ball"].indexOf(root.mood) !== -1
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "squash"; to: 0.965; duration: 1300; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "squash"; to: 1; duration: 1300; easing.type: Easing.InOutSine }
    }

    // respiração profunda
    SequentialAnimation {
        running: root.mood === "sleepy" || root.mood === "sleeping"
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "squash"; to: 0.94; duration: 2300; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "squash"; to: 1; duration: 2300; easing.type: Easing.InOutSine }
    }

    // piscada irregular
    Timer {
        running: root.mood !== "sleepy" && root.mood !== "sleeping"
        repeat: true
        interval: 2400 + Math.random() * 3600
        onTriggered: {
            blinkAnim.restart();
            interval = 2400 + Math.random() * 3600;
        }
    }
    SequentialAnimation {
        id: blinkAnim
        NumberAnimation { target: root; property: "eyeOpen"; to: 0.1; duration: 70 }
        NumberAnimation { target: root; property: "eyeOpen"; to: 1; duration: 90 }
    }

    // olhar vagando
    Timer {
        running: root.mood === "idle"
        repeat: true
        interval: 3000 + Math.random() * 4000
        onTriggered: {
            root.eyeLookX = (Math.random() * 2 - 1) * 1.2;
            interval = 3000 + Math.random() * 4000;
        }
    }
    Behavior on eyeLookX {
        NumberAnimation { duration: 260; easing.type: Easing.OutCubic }
    }

    // ciclo de passos
    NumberAnimation on walkCycle {
        running: root.moving
        loops: Animation.Infinite
        from: 0
        to: 1
        duration: root.mood === "run" ? 260 : 420
    }

    // pulinhos (happy)
    SequentialAnimation {
        running: root.mood === "happy"
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "bounce"; to: -2.6 * root.u; duration: 240; easing.type: Easing.OutQuad }
        NumberAnimation { target: root; property: "bounce"; to: 0; duration: 280; easing.type: Easing.InQuad }
        PauseAnimation { duration: 320 }
    }

    // festa (excited)
    SequentialAnimation {
        running: root.mood === "excited"
        loops: Animation.Infinite
        ParallelAnimation {
            SequentialAnimation {
                NumberAnimation { target: root; property: "bounce"; to: -3.4 * root.u; duration: 180; easing.type: Easing.OutQuad }
                NumberAnimation { target: root; property: "bounce"; to: 0; duration: 200; easing.type: Easing.InQuad }
            }
            SequentialAnimation {
                NumberAnimation { target: root; property: "lean"; to: 6; duration: 95 }
                NumberAnimation { target: root; property: "lean"; to: -6; duration: 190 }
                NumberAnimation { target: root; property: "lean"; to: 0; duration: 95 }
            }
        }
        PauseAnimation { duration: 120 }
    }

    // gingado (dance) — o bounce vem dos cavaLevels, o balanço vem daqui
    SequentialAnimation {
        running: root.mood === "dance"
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "lean"; to: 6; duration: 340; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "lean"; to: -6; duration: 340; easing.type: Easing.InOutSine }
    }
    Binding {
        target: root
        property: "danceBounce"
        value: -root.cavaLevel * 4.5 * root.u
        when: root.mood === "dance"
        restoreMode: Binding.RestoreBindingOrValue
    }
    Behavior on danceBounce {
        enabled: root.mood === "dance"
        NumberAnimation { duration: 70 }
    }

    // tchau (wave)
    SequentialAnimation {
        running: root.mood === "wave"
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "armLift"; to: -105; duration: 260; easing.type: Easing.OutCubic }
        NumberAnimation { target: root; property: "armLift"; to: -65; duration: 220; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "armLift"; to: -105; duration: 220; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "armLift"; to: 0; duration: 300; easing.type: Easing.InCubic }
        PauseAnimation { duration: 260 }
    }

    // cambaleio (sleepy)
    SequentialAnimation {
        running: root.mood === "sleepy"
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "lean"; to: 4; duration: 1500; easing.type: Easing.InOutSine }
        NumberAnimation { target: root; property: "lean"; to: -2; duration: 1500; easing.type: Easing.InOutSine }
    }

    // cacoete (quirk)
    SequentialAnimation {
        running: root.mood === "quirk"
        loops: Animation.Infinite
        NumberAnimation { target: root; property: "eyeLookX"; to: 1.4; duration: 300 }
        PauseAnimation { duration: 420 }
        NumberAnimation { target: root; property: "eyeLookX"; to: -1.4; duration: 300 }
        PauseAnimation { duration: 420 }
        NumberAnimation { target: root; property: "squash"; to: 1.06; duration: 160; easing.type: Easing.OutQuad }
        NumberAnimation { target: root; property: "squash"; to: 1; duration: 200; easing.type: Easing.InQuad }
    }
}

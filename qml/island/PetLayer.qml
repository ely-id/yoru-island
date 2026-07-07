pragma ComponentBehavior: Bound

import QtQuick

// Cena do pet
Item {
    id: root

    property bool showCondition: false
    property bool hovered: false
    property bool musicPlaying: false
    property bool batteryLow: false
    property bool notificationRecent: false
    property var cavaLevels: []
    property real restingHeight: 36
    readonly property real preferredWidth: 200

    readonly property real cavaLevel: {
        const levels = cavaLevels || [];
        if (levels.length === 0) return 0;
        let sum = 0;
        for (let i = 0; i < levels.length; i++) sum += levels[i];
        return Math.min(1, Math.max(0, sum / levels.length));
    }

    readonly property real edgeMargin: 14
    readonly property real leftEdge: edgeMargin
    readonly property real rightEdge: Math.max(edgeMargin, width - body.width - edgeMargin)

    property int ballKicks: 0

    opacity: showCondition ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
    }

    function updateStroll() {
        walkAnim.stop();
        if (brain.walking) {
            strollTo(brain.walkDirection < 0 ? leftEdge : rightEdge);
        } else if (brain.activity !== "ball") {
            strollTo((width - body.width) / 2);
        }
    }

    function strollTo(targetX) {
        const clamped = Math.max(leftEdge, Math.min(rightEdge, targetX));
        if (Math.abs(clamped - body.x) < 2) return;
        brain.walkDirection = clamped < body.x ? -1 : 1;
        walkAnim.to = clamped;
        walkAnim.duration = Math.max(400, Math.abs(clamped - body.x) * (brain.pace === "run" ? 9 : 24));
        walkAnim.start();
    }

    // ---- bola ----
    function startBallGame() {
        ballKicks = 0;
        ball.opacity = 1;
        ball.x = Math.random() < 0.5 ? leftEdge + 2 : rightEdge + body.width - ball.width - 2;
        chaseBall();
    }

    function chaseBall() {
        brain.pace = "run";
        strollTo(ball.x - body.width / 2 + ball.width / 2);
    }

    function kickBall() {
        ballKicks += 1;
        const side = ball.x < root.width / 2 ? 1 : -1;
        ballAnim.to = side > 0
            ? root.width - edgeMargin - ball.width - Math.random() * 20
            : edgeMargin + Math.random() * 20;
        ballAnim.start();
    }

    function endBallGame() {
        ballAnim.stop();
        ball.opacity = 0;
    }

    PetBrain {
        id: brain
        active: root.showCondition
        hovered: root.hovered
        musicPlaying: root.musicPlaying
        batteryLow: root.batteryLow
        notificationRecent: root.notificationRecent
        moving: walkAnim.running

        onWalkingChanged: root.updateStroll()
        onActivityChanged: {
            if (activity === "ball") {
                root.startBallGame();
            } else {
                root.endBallGame();
                if (!walking) root.updateStroll();
            }
        }
    }

    NumberAnimation {
        id: walkAnim
        target: body
        property: "x"
        easing.type: Easing.InOutSine

        onStopped: {
            if (brain.activity === "ball" && ball.opacity > 0) {
                root.kickBall();
                return;
            }
            if (!brain.walking) return;
            if (body.x <= root.leftEdge + 2) root.strollTo(root.rightEdge);
            else if (body.x >= root.rightEdge - 2) root.strollTo(root.leftEdge);
        }
    }

    NumberAnimation {
        id: ballAnim
        target: ball
        property: "x"
        duration: 520
        easing.type: Easing.OutQuad

        onStopped: {
            if (brain.activity !== "ball" || ball.opacity === 0) return;
            if (root.ballKicks >= 4) root.endBallGame();
            else root.chaseBall();
        }
    }

    Rectangle {
        id: ball
        width: 9
        height: 9
        radius: width / 2
        color: "#6F9FD8"
        opacity: 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 3

        Behavior on opacity {
            NumberAnimation { duration: 160 }
        }
    }

    // ---- bolhas de sabão ----
    Timer {
        running: brain.activity === "bubbles" && root.showCondition
        repeat: true
        triggeredOnStart: true
        interval: 650
        onTriggered: bubbleComponent.createObject(root, { startX: body.x + body.width / 2 })
    }

    Component {
        id: bubbleComponent

        Rectangle {
            id: bubble

            property real startX: 0

            width: 4 + Math.random() * 6
            height: width
            radius: width / 2
            color: "transparent"
            border.color: "#6F9FD8"
            border.width: 1.2
            opacity: 0.9
            x: startX
            y: root.height - 14

            ParallelAnimation {
                running: true

                NumberAnimation {
                    target: bubble
                    property: "y"
                    to: 2
                    duration: 1900 + Math.random() * 800
                    easing.type: Easing.OutQuad
                }
                SequentialAnimation {
                    NumberAnimation { target: bubble; property: "x"; to: bubble.startX + (Math.random() * 18 - 9); duration: 950; easing.type: Easing.InOutSine }
                    NumberAnimation { target: bubble; property: "x"; to: bubble.startX + (Math.random() * 18 - 9); duration: 950; easing.type: Easing.InOutSine }
                }
                NumberAnimation {
                    target: bubble
                    property: "opacity"
                    to: 0
                    duration: 2300
                }

                onStopped: bubble.destroy()
            }
        }
    }

    PetBody {
        id: body
        mood: brain.mood
        cavaLevel: root.cavaLevel
        height: Math.min(parent.height, root.restingHeight) - 6
        anchors.verticalCenter: parent.verticalCenter
        x: (root.width - width) / 2

        transform: Scale {
            origin.x: body.width / 2
            xScale: brain.walkDirection < 0 ? -1 : 1
        }
    }
}

pragma ComponentBehavior: Bound

import QtQuick

Item {
    id: root

    property bool showCondition: false
    property bool hovered: false
    property bool musicPlaying: false
    property string sheetSource: ""
    property real restingHeight: 36
    readonly property real preferredWidth: 200

    readonly property real edgeMargin: 14
    readonly property real leftEdge: edgeMargin
    readonly property real rightEdge: Math.max(edgeMargin, width - body.width - edgeMargin)

    opacity: showCondition ? 1 : 0

    Behavior on opacity {
        NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
    }

    function updateStroll() {
        walkAnim.stop();
        if (brain.walking) {
            strollTo(brain.walkDirection < 0 ? leftEdge : rightEdge);
        } else {
            strollTo((width - body.width) / 2);
        }
    }

    function strollTo(targetX) {
        if (Math.abs(targetX - body.x) < 2) return;
        brain.walkDirection = targetX < body.x ? -1 : 1;
        walkAnim.to = targetX;
        walkAnim.duration = Math.max(500, Math.abs(targetX - body.x) * 24);
        walkAnim.start();
    }

    PetBrain {
        id: brain
        active: root.showCondition
        hovered: root.hovered
        musicPlaying: root.musicPlaying
        moving: walkAnim.running

        onWalkingChanged: root.updateStroll()
    }

    NumberAnimation {
        id: walkAnim
        target: body
        property: "x"
        easing.type: Easing.InOutSine

        onStopped: {
            if (!brain.walking) return;
            if (body.x <= root.leftEdge + 2) root.strollTo(root.rightEdge);
            else if (body.x >= root.rightEdge - 2) root.strollTo(root.leftEdge);
        }
    }

    PetBody {
        id: body
        mood: brain.mood
        sheetSource: root.sheetSource
        height: Math.min(parent.height, root.restingHeight) - 8
        anchors.verticalCenter: parent.verticalCenter
        x: (root.width - width) / 2

        transform: Scale {
            origin.x: body.width / 2
            xScale: brain.walkDirection < 0 ? -1 : 1
        }
    }
}
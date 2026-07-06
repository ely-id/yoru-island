import QtQuick
import Quickshell.Wayland

Item {
    id: root

    property bool active: false
    property bool hovered: false
    property bool musicPlaying: false
    property bool moving: false

    property bool walking: false
    property real walkDirection: 1

    readonly property string mood: {
        if (sleepMonitor.isIdle) return "sleeping";
        if (drowsyMonitor.isIdle) return "sleepy";
        if (musicPlaying) return "dance";
        if (hovered) return "happy";
        if (walking || moving) return "walk";
        return "idle";
    }

    IdleMonitor {
        id: drowsyMonitor
        enabled: root.active
        timeout: 180
    }

    IdleMonitor {
        id: sleepMonitor
        enabled: root.active
        timeout: 600
    }

    Timer {
        id: strollTimer
        running: root.active && !drowsyMonitor.isIdle && !root.musicPlaying && !root.hovered
        repeat: true
        interval: 6000 + Math.random() * 8000
        onTriggered: {
            if (root.walking) {
                root.walking = false;
            } else {
                root.walkDirection = Math.random() < 0.5 ? -1 : 1;
                root.walking = true;
            }
            interval = 5000 + Math.random() * 9000;
        }
    }
}
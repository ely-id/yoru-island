import QtQuick
import Quickshell.Wayland

// Cérebro do pet
Item {
    id: root

    property bool active: false
    property bool hovered: false
    property bool musicPlaying: false
    property bool batteryLow: false
    property bool notificationRecent: false

    property int drowsySeconds: 60
    property int sleepSeconds: 600
    property int hydrationIntervalMs: 45 * 60 * 1000

    property string activity: "none"
    property bool walking: false
    property string pace: "walk"
    property real walkDirection: 1
    property bool moving: false
    property bool waveActive: false

    readonly property var quirks: ["quirk", "peek", "catnap", "scratch", "stretch"]

    readonly property bool freeMood: !notificationRecent && !waveActive
        && !sleepMonitor.isIdle && !drowsyMonitor.isIdle
        && !batteryLow && !musicPlaying && !hovered

    readonly property string mood: {
        if (notificationRecent) return "excited";
        if (waveActive) return "wave";
        if (sleepMonitor.isIdle) return "sleeping";
        if (drowsyMonitor.isIdle) return "sleepy";
        if (batteryLow) return "sad";
        if (musicPlaying) return "dance";
        if (hovered) return "happy";
        if (activity === "stroll" || activity === "run" || moving) return pace;
        if (activity === "ball") return "ball";
        if (activity === "bubbles") return "bubbles";
        if (quirks.indexOf(activity) !== -1) return activity;
        return "idle";
    }

    onFreeMoodChanged: if (!freeMood) stopActivity()

    Component.onCompleted: if (active) wave(2500)

    function wave(ms) {
        waveActive = true;
        waveOffTimer.interval = ms;
        waveOffTimer.restart();
    }

    function stopActivity() {
        activityStopTimer.stop();
        activity = "none";
        walking = false;
    }

    function startActivity(kind) {
        pace = kind === "run" ? "run" : "walk";
        activity = kind;
        walking = kind === "stroll" || kind === "run";
        activityStopTimer.interval = activityDuration(kind);
        activityStopTimer.restart();
    }

    function activityDuration(kind) {
        switch (kind) {
        case "stroll": return 6000 + Math.random() * 6000;
        case "run": return 3500 + Math.random() * 3000;
        case "ball": return 11000;
        case "bubbles": return 8000;
        case "quirk": return 3200;
        case "peek": return 4200;
        case "catnap": return 5000;
        case "scratch": return 4500;
        case "stretch": return 3600;
        default: return 4000;
        }
    }

    Timer {
        id: activityPicker
        running: root.active && root.freeMood && root.activity === "none"
        repeat: true
        interval: 4000 + Math.random() * 9000
        onTriggered: {
            root.walkDirection = Math.random() < 0.5 ? -1 : 1;
            const roll = Math.random();
            if (roll < 0.20) root.startActivity("stroll");
            else if (roll < 0.29) root.startActivity("run");
            else if (roll < 0.40) root.startActivity("ball");
            else if (roll < 0.50) root.startActivity("bubbles");
            else if (roll < 0.84) root.startActivity(root.quirks[Math.floor(Math.random() * root.quirks.length)]);
            // senão: fica na dele
            interval = 4000 + Math.random() * 9000;
        }
    }

    Timer {
        id: activityStopTimer
        onTriggered: root.stopActivity()
    }

    IdleMonitor {
        id: drowsyMonitor
        enabled: root.active
        timeout: root.drowsySeconds
    }

    IdleMonitor {
        id: sleepMonitor
        enabled: root.active
        timeout: root.sleepSeconds
    }

    // lembrete de hidratação
    Timer {
        running: root.active
        repeat: true
        interval: root.hydrationIntervalMs
        onTriggered: root.wave(12000)
    }

    Timer {
        id: waveOffTimer
        interval: 12000
        onTriggered: root.waveActive = false
    }
}

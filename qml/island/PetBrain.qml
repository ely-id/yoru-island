import QtQuick
import Quickshell.Wayland

// Cérebro do pet — decide o humor por prioridade e sorteia atividades no tempo livre.
// Não desenha nada; só emite mood/flags para a cena.
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

    property string activity: "none"   // none | stroll | run | ball | bubbles | quirk
    property bool walking: false
    property string pace: "walk"
    property real walkDirection: 1
    property bool moving: false        // injetado pela cena (walkAnim.running)
    property bool waveActive: false

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
        if (activity === "quirk") return "quirk";
        return "idle";
    }

    onFreeMoodChanged: if (!freeMood) stopActivity()

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
            if (roll < 0.28) root.startActivity("stroll");
            else if (roll < 0.40) root.startActivity("run");
            else if (roll < 0.56) root.startActivity("ball");
            else if (roll < 0.70) root.startActivity("bubbles");
            else if (roll < 0.84) root.startActivity("quirk");
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

    // lembrete de hidratação — o tchau periódico
    Timer {
        running: root.active
        repeat: true
        interval: root.hydrationIntervalMs
        onTriggered: {
            root.waveActive = true;
            waveOffTimer.restart();
        }
    }

    Timer {
        id: waveOffTimer
        interval: 12000
        onTriggered: root.waveActive = false
    }
}

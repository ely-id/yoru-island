import QtQuick

Item {
    id: root

    property string mood: "idle"
    property string sheetSource: ""

    readonly property var moodMap: ({
        "idle":     { row: 0, frames: 6, fps: 4 },
        "walk":     { row: 1, frames: 8, fps: 8 },
        "happy":    { row: 3, frames: 4, fps: 6 },
        "dance":    { row: 4, frames: 5, fps: 8 },
        "sad":      { row: 5, frames: 8, fps: 6 },
        "sleepy":   { row: 6, frames: 6, fps: 3 },
        "sleeping": { row: 6, frames: 2, fps: 1 }
    })
    readonly property var currentAnim: moodMap[mood] || moodMap["idle"]

    implicitWidth: height * (192 / 208)

    onCurrentAnimChanged: sprite.restart()

    AnimatedSprite {
        id: sprite
        anchors.fill: parent
        source: root.sheetSource
        frameWidth: 192
        frameHeight: 208
        frameX: 0
        frameY: root.currentAnim.row * 208
        frameCount: root.currentAnim.frames
        frameRate: root.currentAnim.fps
        interpolate: false
        smooth: false
        running: visible
    }
}
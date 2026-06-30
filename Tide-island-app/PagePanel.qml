import QtQuick
import TideIsland 1.0

Rectangle {
    id: pagePanel
    color: "transparent"
    anchors.fill: parent

    function showPage() {
        showAnim.start()
    }

    function hidePage() {
        hideAnim.start()
    }

    SequentialAnimation {
        id: hideAnim

        NumberAnimation {
            target: pagePanel
            property: "opacity"
            to: 0
            duration: Theme.animationDuration
        }

        ScriptAction {
            script: pagePanel.visible = false
        }
    }

    SequentialAnimation {
        id: showAnim

        ScriptAction {
            script: {
                pagePanel.visible = true
                pagePanel.opacity = 0
            }
        }

        NumberAnimation {
            target: pagePanel
            property: "opacity"
            to: 1
            duration: Theme.animationDuration
        }
    }
}

import QtQuick

// The menus' root. Only the page the shell asks for exists; the others are
// unloaded, so a closed menu leaves nothing in the scene graph. Moving
// between launcher screens is the launcher's own business.
Item {
    id: root
    property string screen: "start"

    Loader {
        id: pages
        anchors.fill: parent
        focus: true
        sourceComponent: shell.page === "front" ? (root.screen === "play" ? play : start)
                       : shell.page === "pause" ? pause
                       : null
        onLoaded: item.forceActiveFocus()
    }
    Component {
        id: start
        StartPage {
            focus: true
            onPlay: root.screen = "play"
            onSettings: { }
            onQuit: shell.quit()
        }
    }
    Component { id: play; PlayPage { focus: true; onBack: root.screen = "start" } }
    Component { id: pause; PausePage { focus: true } }
}

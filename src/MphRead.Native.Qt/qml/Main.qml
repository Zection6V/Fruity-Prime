import QtQuick

// The menus' root. Only the page the shell asks for exists; the others are
// unloaded, so a closed menu leaves nothing in the scene graph.
Item {
    id: root

    Loader {
        id: pages
        anchors.fill: parent
        focus: true
        sourceComponent: shell.page === "front" ? front
                       : shell.page === "pause" ? pause
                       : null
        onLoaded: item.forceActiveFocus()
    }
    Component { id: front; FrontPage { focus: true } }
    Component { id: pause; PausePage { focus: true } }
}

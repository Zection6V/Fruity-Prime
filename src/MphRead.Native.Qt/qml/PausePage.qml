import QtQuick

// PauseMenuView: a short card over the match with the deck buttons stacked
// and centred, Resume first and Quit last.
FocusScope {
    id: page
    readonly property real em: Theme.emFor(width, height)

    // UiLayout.Backdrop(overGame): the scrim; then the sheet's own.
    Rectangle { anchors.fill: parent; color: Theme.scrim }
    Rectangle { anchors.fill: parent; color: Qt.rgba(5 / 255, 7 / 255, 10 / 255, 184 / 255) }

    // SheetPad: 0.9 em at the sides, 1.1 above and below.
    Item {
        anchors.fill: parent
        anchors.leftMargin: Theme.roundEven(page.em * 0.9); anchors.rightMargin: anchors.leftMargin
        anchors.topMargin: Theme.roundEven(page.em * 1.1); anchors.bottomMargin: anchors.topMargin

        DeckCard {
            id: card
            em: page.em
            maxWidthEms: 19   // UiLayout.WellShort
            anchors.centerIn: parent
            width: implicitWidth
            height: implicitHeight
            // FitToHost: never taller than the host, down to half size.
            scale: Math.max(0.5, Math.min(1, parent.height / (menu.count * 26 + (menu.count - 1) * 6 + 44 + 84 + 70)))

            Column {
                id: menu
                readonly property int count: children.length
                width: parent.width
                spacing: 6
                component Entry: DeckButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    em: page.em; sizeEms: 1.2; padXEms: 0.8; padYEms: 0.5; lip: 4
                    face: Theme.slate
                }
                Entry { id: resume; text: "Resume"; face: Theme.moss; focus: true; onClicked: shell.resume()
                        KeyNavigation.down: fullscreen }
                Entry { id: fullscreen; text: shell.windowLabel; onClicked: shell.toggleFullscreen()
                        KeyNavigation.up: resume; KeyNavigation.down: settings }
                Entry { id: settings; text: "Settings"; onClicked: shell.openSettings()
                        KeyNavigation.up: fullscreen; KeyNavigation.down: leave }
                Entry { id: leave; text: "Leave match"; face: Theme.brass; onClicked: shell.leaveMatch()
                        KeyNavigation.up: settings; KeyNavigation.down: quit }
                Entry { id: quit; text: "Quit"; face: Theme.rust; onClicked: shell.quit()
                        KeyNavigation.up: leave }
            }
        }
    }
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Up || event.key === Qt.Key_Down || event.key === Qt.Key_Tab)
            Theme.keyboardDriving = true
    }
    Keys.onEscapePressed: shell.resume()
}

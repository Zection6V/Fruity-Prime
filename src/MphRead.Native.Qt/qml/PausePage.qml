import QtQuick

// The in-match menu over the running game.
Item {
    id: page
    Rectangle { anchors.fill: parent; color: "#a6000000" }

    Rectangle {
        anchors.centerIn: parent
        width: 360
        height: column.implicitHeight + 48
        radius: Theme.radius
        color: Theme.panel
        border.color: Theme.line

        Column {
            id: column
            anchors.centerIn: parent
            spacing: Theme.gap
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "PAUSED"
                color: Theme.ink
                font.family: Theme.font; font.pixelSize: 26; font.weight: Font.Black
            }
            UiButton {
                id: resume
                width: 280
                text: "Resume"
                primary: true
                focus: true
                KeyNavigation.down: leave
                onClicked: shell.resume()
            }
            UiButton {
                id: leave
                width: 280
                text: "Leave match"
                KeyNavigation.down: quit
                onClicked: shell.leaveMatch()
            }
            UiButton {
                id: quit
                width: 280
                text: "Quit game"
                onClicked: shell.quit()
            }
        }
    }
    Keys.onEscapePressed: shell.resume()
}

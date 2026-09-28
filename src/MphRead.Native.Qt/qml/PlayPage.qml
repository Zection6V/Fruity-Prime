import QtQuick

// Stand-in for PlayScreen until its Offline tab (the map cards) is ported:
// pick an arena, a mode and opponents, then play offline.
Item {
    id: page
    signal back()

    Image {
        anchors.fill: parent
        source: "launcher-bg.jpg"
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        sourceSize.width: page.width
    }
    Rectangle { anchors.fill: parent; color: "#8c05070b" }

    Column {
        anchors.left: parent.left
        anchors.leftMargin: Math.max(24, parent.width * 0.07)
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.gap

        Text {
            text: "FRUITY PRIME"
            color: Theme.ink
            font.family: Theme.font; font.pixelSize: 44; font.weight: Font.Black
            font.letterSpacing: 2
        }
        Text {
            visible: !shell.gameFilesReady
            text: "Game files are not set up yet."
            color: Theme.accent
            font.family: Theme.font; font.pixelSize: 16
        }
        Item { width: 1; height: 8 }

        UiChoice {
            id: room
            focus: true
            label: "Arena"
            model: shell.rooms.map(r => ({ name: r.name, value: r.key }))
            KeyNavigation.down: mode
        }
        UiChoice {
            id: mode
            label: "Mode"
            model: shell.modes
            KeyNavigation.down: hunter
        }
        UiChoice {
            id: hunter
            label: "Hunter"
            model: shell.hunters
            KeyNavigation.down: bots
        }
        UiChoice {
            id: bots
            label: "Bots"
            model: [0, 1, 2, 3, 4, 5, 6, 7].map(n => ({ name: String(n), value: n }))
            index: 3
            KeyNavigation.down: level
        }
        UiChoice {
            id: level
            label: "Bot level"
            model: [{ name: "Easy", value: 0 }, { name: "Medium", value: 1 },
                    { name: "Hard", value: 2 }, { name: "Insane", value: 3 }]
            index: 1
            KeyNavigation.down: play
        }
        Item { width: 1; height: 8 }
        Row {
            spacing: Theme.gap
            UiButton {
                id: play
                text: "Play"
                primary: true
                enabled: shell.rooms.length > 0
                KeyNavigation.right: quit
                onClicked: shell.play(room.value, mode.value, hunter.value, bots.value, level.value)
            }
            UiButton {
                id: quit
                text: "Back"
                onClicked: page.back()
            }
        }
    }
    Keys.onEscapePressed: page.back()
}

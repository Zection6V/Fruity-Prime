import QtQuick

// StartScreen: the wordmark over the backdrop, Play/Settings/Quit along the
// foot between the profile chip and the support mark.
FocusScope {
    id: page
    signal play()
    signal settings()
    signal quit()

    readonly property real em: Theme.emFor(width, height)
    // StartScreen.BarTurnsWidth: narrower than this the bar stacks.
    readonly property bool column: width < 470

    Backdrop { anchors.fill: parent }

    Column {
        anchors.centerIn: parent
        spacing: 0
        DeckWordmark {
            anchors.horizontalCenter: parent.horizontalCenter
            em: page.em
            sizeEms: Theme.phone ? (page.width > page.height ? 4.4 : 5.2) : 7.6
        }
        Text {
            id: subtitle
            anchors.horizontalCenter: parent.horizontalCenter
            readonly property int fontSize: Math.max(8, Math.round(page.em * 0.82))
            topPadding: Math.round(page.em * 0.82 * 1.4) - 10
            text: "METROID PRIME HUNTERS  ·  REBORN"
            font.family: Theme.pixel
            font.pixelSize: fontSize
            color: Theme.textDim
        }
    }

    // The foot: chip | bar | heart, bottom-aligned.
    Item {
        id: foot
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.leftMargin: page.column ? 14 : 26
        anchors.rightMargin: page.column ? 14 : 26
        anchors.bottomMargin: page.column ? 18 : 24
        height: Math.max(chip.height, bar.height, heart.height)

        DeckChip {
            id: chip
            anchors.bottom: parent.bottom
            key: "Profile"
            value: shell.playerName
        }
        Row {
            id: bar
            anchors.bottom: parent.bottom
            // Centred in the grid's middle column, between chip and heart.
            x: chip.width + Theme.roundEven((foot.width - chip.width - heart.width - width) / 2)
            spacing: 12
            DeckButton {
                id: playButton
                text: "PLAY"; face: Theme.blue; em: page.em; idle: true
                focus: true
                KeyNavigation.right: settingsButton
                onClicked: page.play()
            }
            DeckButton {
                id: settingsButton
                text: "SETTINGS"; face: Theme.brass; em: page.em
                KeyNavigation.left: playButton; KeyNavigation.right: quitButton
                onClicked: page.settings()
            }
            DeckButton {
                id: quitButton
                text: "QUIT"; face: Theme.rust; em: page.em
                KeyNavigation.left: settingsButton; KeyNavigation.right: heart
                onClicked: page.quit()
            }
        }
        DeckButton {
            id: heart
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            face: Theme.rust; em: page.em
            sizeEms: 1.55; padXEms: 0.9; padYEms: 0.7; lip: 5
            heart: true
            tip: "Support this project <3"
            KeyNavigation.left: quitButton
            onClicked: shell.openSupport()
        }
    }

    Text {
        anchors.right: parent.right; anchors.top: parent.top
        anchors.rightMargin: 24; anchors.topMargin: 18
        text: shell.version
        font.family: Theme.pixel; font.pixelSize: 12
        color: Theme.textDim
    }
    Text {
        anchors.left: parent.left; anchors.bottom: parent.bottom
        anchors.leftMargin: 20; anchors.bottomMargin: 3
        text: "Enter Select   Esc Back"
        font.pixelSize: 12
        color: Theme.textDim
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Right || event.key === Qt.Key_Tab)
            Theme.keyboardDriving = true
    }
    Keys.onEscapePressed: page.quit()
}

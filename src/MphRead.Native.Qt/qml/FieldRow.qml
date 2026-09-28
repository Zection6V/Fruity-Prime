import QtQuick

// FieldRow: a label and a Fluent text box held to the right.
FocusScope {
    id: row
    property string label
    property alias text: input.text
    property real boxWidth: 150
    signal edited(string text)
    implicitWidth: 300
    implicitHeight: 36
    height: implicitHeight

    Text {
        x: 4
        anchors.verticalCenter: parent.verticalCenter
        text: row.label
        font.family: Theme.pixel; font.pixelSize: 13
        color: Theme.textDim
    }
    // The Fluent dark TextBox: a faint fill, a light hairline and, with the
    // keyboard, the accent along its foot.
    Rectangle {
        id: box
        x: row.width - width
        width: row.boxWidth
        height: Math.round(metrics.height) + 8 + 2
        anchors.verticalCenter: parent.verticalCenter
        radius: 4
        color: input.activeFocus ? "#1f1f1f" : Qt.rgba(1, 1, 1, 0.045)
        border.width: 1
        border.color: input.activeFocus ? Qt.rgba(1, 1, 1, 0.08) : Qt.rgba(1, 1, 1, 0.55)
        Rectangle {
            visible: input.activeFocus
            x: 1; width: parent.width - 2
            y: parent.height - 2; height: 2
            color: Theme.accent
        }
        FontMetrics { id: metrics; font: input.font }
        TextInput {
            id: input
            focus: true
            x: 8; width: parent.width - 16
            anchors.verticalCenter: parent.verticalCenter
            font.family: Theme.pixel; font.pixelSize: 13
            color: Theme.text
            selectionColor: Qt.rgba(1, 179 / 255, 71 / 255, 90 / 255)
            selectedTextColor: Theme.text
            clip: true
            onTextEdited: row.edited(text)
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.IBeamCursor
            onPressed: mouse => { input.forceActiveFocus(); mouse.accepted = false }
        }
    }
}

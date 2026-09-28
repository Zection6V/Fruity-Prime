import QtQuick

// A flat button: no layers, no shaders, no animation while idle.
Rectangle {
    id: root
    property alias text: label.text
    property bool primary: false
    signal clicked()

    implicitWidth: Math.max(160, label.implicitWidth + 40)
    implicitHeight: 44
    radius: Theme.radius
    color: primary ? (area.pressed ? Qt.darker(Theme.accent, 1.2) : Theme.accent)
                   : (area.containsMouse || activeFocus ? Theme.line : Theme.field)
    border.width: activeFocus ? 2 : 0
    border.color: Theme.ink
    activeFocusOnTab: true

    Text {
        id: label
        anchors.centerIn: parent
        color: root.primary ? Theme.accentInk : Theme.ink
        font.family: Theme.font
        font.pixelSize: 17
        font.weight: Font.DemiBold
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: root.clicked()
    }
    Keys.onReturnPressed: root.clicked()
    Keys.onEnterPressed: root.clicked()
    Keys.onSpacePressed: root.clicked()
}

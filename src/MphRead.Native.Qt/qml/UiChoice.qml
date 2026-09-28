import QtQuick

// A labelled row that steps through a list of {name, value} with the arrows.
Rectangle {
    id: root
    property string label
    property var model: []
    property int index: 0
    readonly property var value: model.length > 0 ? model[index].value : undefined

    implicitWidth: 420
    implicitHeight: 44
    radius: Theme.radius
    color: activeFocus ? Theme.line : Theme.field
    activeFocusOnTab: true

    function step(delta) {
        if (model.length > 0)
            index = (index + delta + model.length) % model.length
    }

    Text {
        anchors.left: parent.left; anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        text: root.label
        color: Theme.muted
        font.family: Theme.font; font.pixelSize: 15
    }
    Row {
        anchors.right: parent.right; anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        spacing: 4
        Text {
            text: "‹"; color: Theme.ink; font.pixelSize: 22
            MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.step(-1) }
        }
        Text {
            width: 190
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: root.model.length > 0 ? root.model[root.index].name : "—"
            color: Theme.ink
            font.family: Theme.font; font.pixelSize: 16; font.weight: Font.DemiBold
        }
        Text {
            text: "›"; color: Theme.ink; font.pixelSize: 22
            MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.step(1) }
        }
    }
    Keys.onLeftPressed: step(-1)
    Keys.onRightPressed: step(1)
}

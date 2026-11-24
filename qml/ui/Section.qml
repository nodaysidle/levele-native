import QtQuick
import QtQuick.Controls

// Collapsible inspector section. The header is a real button: Tab reaches it,
// Space/Enter toggles it, and a screen reader reads the title and state.
Column {
    id: root
    property string title: ""
    property bool expanded: true
    default property alias content: body.data

    width: parent ? parent.width : 200
    spacing: 8

    AbstractButton {
        id: header
        width: parent.width
        height: 24
        focusPolicy: Qt.StrongFocus
        hoverEnabled: true

        Accessible.role: Accessible.Button
        Accessible.name: root.title
        Accessible.checkable: true
        Accessible.checked: root.expanded
        Accessible.onPressAction: root.expanded = !root.expanded

        contentItem: Row {
            spacing: 6
            Text {
                text: root.expanded ? "\u25BE" : "\u25B8"
                color: theme.muted
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: root.title
                color: header.hovered || header.activeFocus ? theme.fg : theme.muted
                font.family: "Noto Sans"
                font.pixelSize: 12
                font.weight: Font.Medium
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        background: Rectangle {
            color: "transparent"
            Rectangle {
                visible: header.visualFocus
                anchors.fill: parent
                anchors.margins: -2
                color: "transparent"
                border.width: 1
                border.color: theme.accent
                radius: Math.min(4, theme.radius)
            }
        }

        onClicked: root.expanded = !root.expanded
    }

    Column {
        id: body
        width: parent.width
        spacing: 8
        visible: root.expanded
    }
}

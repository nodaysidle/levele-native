import QtQuick
import QtQuick.Controls

CheckBox {
    id: control
    property string label: ""

    text: label
    font.family: "Noto Sans"
    font.pixelSize: 12
    implicitHeight: 24
    spacing: 8
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.CheckBox
    Accessible.name: label
    padding: 0

    contentItem: Text {
        text: control.text
        color: control.enabled ? theme.fg : theme.muted
        font: control.font
        leftPadding: control.indicator.width + 8
        verticalAlignment: Text.AlignVCenter
    }

    indicator: Rectangle {
        x: 0
        y: (control.height - height) / 2
        implicitWidth: 16
        implicitHeight: 16
        radius: Math.min(4, theme.radius)
        color: control.checked ? theme.accent : theme.bg
        border.width: 1
        border.color: control.checked || control.hovered ? theme.accent : theme.border

        Rectangle {
            visible: control.visualFocus
            anchors.fill: parent
            anchors.margins: -3
            radius: Math.min(4, theme.radius) + 3
            color: "transparent"
            border.width: 2
            border.color: theme.accent
        }

        Text {
            anchors.centerIn: parent
            visible: control.checked
            text: "\u2713"
            color: theme.accentInk
            font.pixelSize: 11
            font.weight: Font.Bold
        }
    }
}

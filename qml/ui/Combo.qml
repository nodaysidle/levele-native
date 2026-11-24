import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    property int controlWidth: 120
    property string label: ""   // Accessible.name; falls back to the shown text

    implicitWidth: controlWidth
    implicitHeight: 30
    font.family: "Noto Sans"
    font.pixelSize: 12
    leftPadding: 10
    rightPadding: 24
    focusPolicy: Qt.StrongFocus
    Accessible.role: Accessible.ComboBox
    Accessible.name: label !== "" ? label : displayText

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: theme.fg
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    indicator: Text {
        x: control.width - 18
        y: (control.height - height) / 2
        text: "\u25BE"
        color: theme.muted
        font.pixelSize: 11
    }

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: theme.bg
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus || control.hovered ? theme.accent : theme.border
    }

    delegate: ItemDelegate {
        width: control.width
        height: 28
        contentItem: Text {
            text: modelData !== undefined ? modelData : ""
            color: highlighted ? theme.accentInk : theme.fg
            font.family: "Noto Sans"
            font.pixelSize: 12
            verticalAlignment: Text.AlignVCenter
            leftPadding: 8
            elide: Text.ElideRight
        }
        background: Rectangle {
            color: highlighted ? theme.accent : "transparent"
            radius: Math.min(4, theme.radius)
        }
        highlighted: control.highlightedIndex === index
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: contentItem.implicitHeight + 8
        padding: 4
        background: Rectangle {
            color: theme.surface
            border.color: theme.border
            border.width: 1
            radius: theme.radius
        }
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            boundsBehavior: Flickable.StopAtBounds
        }
    }
}

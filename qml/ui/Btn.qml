import QtQuick
import QtQuick.Controls

Button {
    id: control
    property string variant: "default"   // default | accent | ghost | danger
    property int radius: Math.min(8, theme.radius)
    property string tip: ""              // tooltip text
    property string label: ""            // Accessible.name; falls back to text

    hoverEnabled: true
    implicitHeight: 32
    leftPadding: 12
    rightPadding: 12
    font.family: "Noto Sans"
    font.pixelSize: 13
    font.weight: Font.Medium
    focusPolicy: Qt.StrongFocus

    Accessible.role: Accessible.Button
    Accessible.name: label !== "" ? label : text
    Accessible.description: tip
    Accessible.checkable: control.checkable
    Accessible.checked: control.checked
    Accessible.onPressAction: control.clicked()

    ToolTip.visible: hovered && tip !== ""
    ToolTip.text: tip
    ToolTip.delay: 500

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.variant === "accent" ? theme.accentInk
             : control.variant === "danger" ? theme.red
             : theme.fg
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.45
    }

    background: Rectangle {
        radius: control.radius
        color: {
            if (control.variant === "accent")
                return control.down ? Qt.darker(theme.accent, 1.15)
                     : control.hovered ? Qt.lighter(theme.accent, 1.08) : theme.accent
            if (control.variant === "ghost")
                return control.down ? theme.surface : control.hovered ? theme.surfaceAlt : "transparent"
            return control.down ? theme.bg : control.hovered ? theme.surfaceAlt : theme.surface
        }
        border.width: control.variant === "accent" || control.variant === "ghost" ? 0 : 1
        border.color: theme.border
        opacity: control.enabled ? 1 : 0.45

        // Keyboard focus ring, drawn outside the fill with theme.accent so it
        // keeps >=3:1 contrast against surface/chrome on all themes.
        Rectangle {
            visible: control.visualFocus
            anchors.fill: parent
            anchors.margins: control.variant === "accent" ? -4 : -3
            radius: control.radius + (control.variant === "accent" ? 4 : 3)
            color: "transparent"
            border.width: 2
            border.color: theme.accent
        }
    }
}

import QtQuick
import QtQuick.Controls

// Glyph-only control: Tab-reachable, keyboard-operable, named for screen
// readers and at least 24x24 px, with a tooltip. The glyph stays small so dense
// toolbars keep their look; the hit area does not.
AbstractButton {
    id: control

    property string glyph: ""
    property string tip: ""
    property string label: ""            // Accessible.name; falls back to tip
    property bool picked: false
    property bool toggleable: false
    property color glyphColor: picked ? theme.accentInk : theme.fg
    property int glyphSize: 13
    property real cornerRadius: Math.min(6, theme.radius)

    implicitWidth: 28
    implicitHeight: 28
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: label !== "" ? label : (tip !== "" ? tip : glyph)
    Accessible.description: tip
    Accessible.checkable: control.toggleable
    Accessible.checked: control.toggleable && control.picked
    Accessible.onPressAction: control.clicked()

    ToolTip.visible: hovered && tip !== ""
    ToolTip.text: tip
    ToolTip.delay: 500

    contentItem: Text {
        text: control.glyph
        color: control.glyphColor
        font.pixelSize: control.glyphSize
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        opacity: control.enabled ? 1 : 0.4
    }

    background: Rectangle {
        radius: control.cornerRadius
        color: control.picked ? theme.accent
             : control.down ? theme.surfaceAlt
             : control.hovered ? theme.surfaceAlt : "transparent"
        border.width: control.picked && !control.visualFocus ? 1 : 0
        border.color: theme.border

        // Focus ring outside the control using theme.accent for contrast.
        Rectangle {
            visible: control.visualFocus
            anchors.fill: parent
            anchors.margins: control.picked ? -4 : -3
            radius: control.cornerRadius + (control.picked ? 4 : 3)
            color: "transparent"
            border.width: 2
            border.color: theme.accent
        }
    }
}

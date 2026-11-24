import QtQuick
import QtQuick.Controls

// Text-alignment button: four rules laid out to show the alignment, so left,
// centre, right and justify are told apart at a glance instead of all sharing
// one glyph.
AbstractButton {
    id: control
    property string mode: "left"    // left | center | right | justify
    property string label: ""
    property bool picked: false

    implicitWidth: 28
    implicitHeight: 28
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.checkable: true
    Accessible.checked: picked
    Accessible.onPressAction: control.clicked()

    ToolTip.visible: hovered && label !== ""
    ToolTip.text: label
    ToolTip.delay: 500

    contentItem: Item {
        anchors.fill: parent

        Column {
            anchors.fill: parent
            anchors.margins: 7
            spacing: 3

            Repeater {
                model: 4
                delegate: Item {
                    required property int index
                    width: parent.width
                    height: 2

                    Rectangle {
                        height: 1.5
                        color: control.picked ? theme.accentInk : theme.fg
                        width: control.mode === "justify" ? parent.width
                             : parent.width * (index === 1 ? 0.62 : index === 3 ? 0.55 : 1.0)
                        x: control.mode === "left" ? 0
                         : control.mode === "right" ? parent.width - width
                         : control.mode === "center" ? (parent.width - width) / 2
                         : 0
                    }
                }
            }
        }
    }

    background: Rectangle {
        radius: Math.min(4, theme.radius)
        color: control.picked ? theme.accent
             : control.down ? theme.surfaceAlt
             : control.hovered ? theme.surfaceAlt : "transparent"
        border.width: control.picked && !control.visualFocus ? 1 : 0
        border.color: theme.accent

        Rectangle {
            visible: control.visualFocus
            anchors.fill: parent
            anchors.margins: control.picked ? -4 : -3
            radius: Math.min(4, theme.radius) + (control.picked ? 4 : 3)
            color: "transparent"
            border.width: 2
            border.color: theme.accent
        }
    }
}

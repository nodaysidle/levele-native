import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

// Colour swatch with a small themed popup: hex entry, the current theme's
// palette, and the platform picker for anything else. Keyboard reachable and
// named with the current value so a screen reader can tell chips apart.
AbstractButton {
    id: root
    property color value: "#000000"
    property bool allowAlpha: false
    property string label: qsTr("Colour")

    signal picked(color c)

    implicitWidth: 44
    implicitHeight: 30
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    Accessible.role: Accessible.Button
    Accessible.name: label + " " + String(value)
    Accessible.onPressAction: root.clicked()
    ToolTip.visible: hovered && !popup.visible
    ToolTip.text: label + " · " + String(value)
    ToolTip.delay: 500

    function applyHex(text) {
        var t = String(text).trim()
        if (t.charAt(0) !== "#")
            t = "#" + t
        if (/^#[0-9a-fA-F]{6}$/.test(t)) {
            root.picked(t)
            popup.close()
        } else {
            hexField.text = String(root.value).toUpperCase()
            hexField.forceActiveFocus()
            hexField.selectAll()
        }
    }

    contentItem: Rectangle {
        radius: Math.min(4, theme.radius)
        color: root.value
        border.width: 1
        border.color: root.hovered || root.activeFocus || popup.visible ? theme.accent : theme.border

        Rectangle {
            visible: root.visualFocus
            anchors.fill: parent
            anchors.margins: -3
            radius: Math.min(4, theme.radius) + 3
            color: "transparent"
            border.width: 2
            border.color: theme.accent
        }
    }

    onClicked: popup.visible ? popup.close() : popup.open()

    Popup {
        id: popup
        x: root.parent ? Math.max(-root.x, Math.min(0, root.parent.width - root.x - width)) : 0
        y: root.height + 6
        width: 232
        padding: 12
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            color: theme.surface
            radius: theme.radius
            border.width: 1
            border.color: theme.border
        }

        contentItem: Column {
            spacing: 10

            Row {
                spacing: 6
                Repeater {
                    model: [
                        { label: qsTr("Accent"), colour: theme.accent },
                        { label: qsTr("Foreground"), colour: theme.fg },
                        { label: qsTr("Background"), colour: theme.bg },
                        { label: qsTr("Surface"), colour: theme.surfaceAlt },
                        { label: qsTr("Red"), colour: theme.red },
                        { label: qsTr("Green"), colour: theme.green },
                        { label: qsTr("Yellow"), colour: theme.yellow }
                    ]
                    delegate: AbstractButton {
                        id: swatchBtn
                        width: 26
                        height: 26
                        focusPolicy: Qt.StrongFocus
                        hoverEnabled: true
                        Accessible.role: Accessible.Button
                        Accessible.name: qsTr("Use the theme ") + modelData.label.toLowerCase()
                        Accessible.onPressAction: root.picked(modelData.colour)
                        contentItem: Rectangle {
                            radius: Math.min(4, theme.radius)
                            color: modelData.colour
                            border.width: 1
                            border.color: swatchBtn.hovered ? theme.accent : theme.border
                            Rectangle {
                                visible: swatchBtn.visualFocus
                                anchors.fill: parent
                                anchors.margins: -3
                                radius: Math.min(4, theme.radius) + 3
                                color: "transparent"
                                border.width: 2
                                border.color: theme.accent
                            }
                        }
                        onClicked: root.picked(modelData.colour)
                    }
                }
            }

            Row {
                spacing: 6
                TextField {
                    id: hexField
                    width: 118
                    height: 28
                    Accessible.name: root.label + " hex value"
                    text: String(root.value).toUpperCase()
                    color: theme.fg
                    font.family: "JetBrains Mono"
                    font.pixelSize: 12
                    leftPadding: 8
                    selectByMouse: true
                    onAccepted: root.applyHex(text)
                    background: Rectangle {
                        radius: Math.min(4, theme.radius)
                        color: theme.bg
                        border.width: hexField.activeFocus ? 2 : 1
                        border.color: hexField.activeFocus ? theme.accent : theme.border
                    }
                }
                Btn {
                    text: qsTr("Set")
                    width: 48
                    height: 28
                    font.pixelSize: 12
                    Accessible.name:  qsTr("Apply the hex value")
                    onClicked: root.applyHex(hexField.text)
                }
                Btn {
                    text: "\u2026"
                    width: 34
                    height: 28
                    font.pixelSize: 12
                    Accessible.name:  qsTr("Open the colour picker")
                    tip: qsTr("Open the system colour picker")
                    onClicked: {
                        popup.close()
                        dialog.open()
                    }
                }
            }
        }

        onOpened: {
            hexField.text = String(root.value).toUpperCase()
            hexField.forceActiveFocus()
            hexField.selectAll()
        }
    }

    ColorDialog {
        id: dialog
        title: root.label
        selectedColor: root.value
        options: root.allowAlpha ? ColorDialog.ShowAlphaChannel : 0
        onAccepted: root.picked(selectedColor)
    }
}

import QtQuick
import QtQuick.Controls
import "ui"

// Every binding the editor actually handles: the canvas keys in
// CanvasItem::keyPressEvent and the window shortcuts in Main.qml. Keep this
// table and those two code paths in step — the overlay used to list only half
// of them.
Dialog {
    id: dlg
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    modal: true
    width: Math.min(540, Overlay.overlay ? Overlay.overlay.width - 32 : 540)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    readonly property var shortcuts: [
        { keys: qsTr("V / R / U / O / Y"), desc: qsTr("Select, rectangle, rounded rectangle, ellipse, triangle") },
        { keys: qsTr("P / L / A"), desc: qsTr("Star, line, arrow") },
        { keys: qsTr("T / I"), desc: qsTr("Text, place an image") },
        { keys: qsTr("G"), desc: qsTr("Toggle the 50 px grid") },
        { keys: qsTr("Space + drag"), desc: qsTr("Pan the canvas") },
        { keys: qsTr("Wheel"), desc: qsTr("Pan · Ctrl + wheel zooms around the pointer") },
        { keys: qsTr("Ctrl + 0 / + / -"), desc: qsTr("Fit the page, zoom in, zoom out") },
        { keys: qsTr("Arrow keys"), desc: qsTr("Nudge the selection 1 px (Shift: 10 px)") },
        { keys: qsTr("Delete / Backspace"), desc: qsTr("Delete the selection") },
        { keys: qsTr("Shift + drag"), desc: qsTr("Constrain a shape; 45° steps for lines and arrows") },
        { keys: qsTr("Shift + rotate"), desc: qsTr("Rotate in 15° steps") },
        { keys: qsTr("Esc"), desc: qsTr("Clear the selection (or close a dialog)") },
        { keys: qsTr("Enter / double-click"), desc: qsTr("Edit the text of a text element") },
        { keys: qsTr("Shift + Enter"), desc: qsTr("New line while editing text") },
        { keys: qsTr("Esc (editing)"), desc: qsTr("Cancel the text edit") },
        { keys: qsTr("Ctrl + Z / Ctrl + Shift + Z"), desc: qsTr("Undo / redo") },
        { keys: qsTr("Ctrl + D / A"), desc: qsTr("Duplicate, select all") },
        { keys: qsTr("Ctrl + S / Ctrl + E"), desc: qsTr("Save, export") },
        { keys: qsTr("Ctrl + 1 / Ctrl + 2"), desc: qsTr("Show or hide the panels, the inspector") },
        { keys: qsTr("[ / ]"), desc: qsTr("Same as Ctrl + 1 / Ctrl + 2") },
        { keys: qsTr("Ctrl + Shift + ← / →"), desc: qsTr("Move the current page") },
        { keys: qsTr("← / → on a page"), desc: qsTr("Previous / next page") },
        { keys: qsTr("Alt + ↑ / ↓ on a layer"), desc: qsTr("Raise or lower the layer") },
        { keys: qsTr("?"), desc: qsTr("Toggle this list") },
        { keys: qsTr("Q"), desc: qsTr("Save and quit") }
    ]

    background: Rectangle {
        color: theme.surface
        radius: theme.radius
        border.width: 1
        border.color: theme.border
    }

    contentItem: Item {
        implicitWidth: dlg.width
        implicitHeight: Math.min(body.implicitHeight + 40,
                                 Overlay.overlay ? Math.max(200, Overlay.overlay.height - 32) : 620,
                                 620)

        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: body.implicitHeight + 40
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            Column {
                id: body
                x: 20
                y: 20
                width: parent.width - 40
                spacing: 14

                Text {
                    text: qsTr("Keyboard shortcuts")
                    Accessible.name:  qsTr("Keyboard shortcuts")
                    Accessible.role: Accessible.Heading
                    color: theme.fg
                    font.family: "Noto Sans"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                    width: parent.width
                }

                Column {
                    width: parent.width
                    spacing: 6

                    Repeater {
                        model: dlg.shortcuts
                        delegate: Row {
                            width: parent.width
                            height: Math.max(26, descText.implicitHeight)
                            spacing: 12
                            Accessible.name: modelData.keys + ": " + modelData.desc

                            Rectangle {
                                width: 190
                                height: 24
                                anchors.verticalCenter: parent.verticalCenter
                                radius: Math.min(4, theme.radius)
                                color: theme.bg
                                border.width: 1
                                border.color: theme.border

                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - 12
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                    text: modelData.keys
                                    color: theme.fg
                                    font.family: "JetBrains Mono"
                                    font.pixelSize: 11
                                }
                            }

                            Text {
                                id: descText
                                width: parent.width - 202
                                text: modelData.desc
                                color: theme.muted
                                font.family: "Noto Sans"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }

                Row {
                    width: parent.width
                    Btn {
                        text: qsTr("Close")
                        width: 72
                        height: 28
                        font.pixelSize: 12
                        Accessible.name:  qsTr("Close the shortcut list")
                        onClicked: dlg.close()
                    }
                }
            }
        }
    }
}

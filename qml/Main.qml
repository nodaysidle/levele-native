import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "ui"

ApplicationWindow {
    id: win
    width: 1440
    height: 900
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: studio.editing ? (studio.title + " \u2014 Levele") : "Levele"
    color: theme.chrome
    readonly property bool modalOpen: leaveDialog.visible
                                      || shortcutsOverlay.visible
                                      || (screen.item && screen.item.exportOpen === true)

    function commitEditor() {
        win.contentItem.forceActiveFocus()
        if (screen.item && screen.item.commitEdits)
            screen.item.commitEdits()
    }

    Connections {
        target: studio
        function onClosingSoon() { win.commitEditor() }
        function onLeaveBlocked(quitting) {
            leaveDialog.quitting = quitting
            leaveDialog.message = studio.status
            leaveDialog.open()
        }
    }

    Loader {
        id: screen
        anchors.fill: parent
        source: studio.editing ? "Editor.qml" : "Dashboard.qml"
    }

    Dialog {
        id: leaveDialog
        objectName: "leaveDialog"
        parent: Overlay.overlay
        anchors.centerIn: Overlay.overlay
        width: Math.min(440, win.width - 32)
        modal: true
        padding: 20
        closePolicy: Popup.CloseOnEscape
        title: qsTr("Unsaved changes")
        property bool quitting: false
        property string message: ""

        function leave() {
            if (quitting ? win.close() : studio.closeDoc())
                close()
        }

        background: Rectangle {
            color: theme.surface
            radius: theme.radius
            border.width: 1
            border.color: theme.border
        }

        header: Item {
            implicitHeight: 40
            Text {
                x: 20
                y: 16
                text: leaveDialog.title
                Accessible.role: Accessible.Heading
                Accessible.name: leaveDialog.title
                color: theme.fg
                font.family: "Noto Sans"
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
        }

        contentItem: Label {
            text: leaveDialog.message
            color: theme.fg
            font.family: "Noto Sans"
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }

        footer: Item {
            implicitHeight: 52
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Btn { text: qsTr("Cancel"); onClicked: leaveDialog.close() }
                Btn {
                    text: qsTr("Discard changes")
                    variant: "danger"
                    onClicked: {
                        studio.discardChanges()
                        leaveDialog.leave()
                    }
                }
                Btn { text: qsTr("Retry save"); variant: "accent"; onClicked: leaveDialog.leave() }
            }
        }
    }

    // ---- status toast
    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(toastRow.implicitWidth + 40, win.width - 32)
        height: 36
        radius: theme.radius
        color: theme.surface
        border.width: 1
        border.color: theme.border
        opacity: studio.status !== "" ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 160 } }

        Row {
            id: toastRow
            anchors.centerIn: parent
            spacing: 12

            Text {
                id: label
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, Math.max(80, win.width - 72 - (undoDeleteButton.visible ? undoDeleteButton.width + 12 : 0)))
                Accessible.name: studio.status
                Accessible.role: Accessible.StaticText
                text: studio.status
                color: theme.fg
                font.family: "Noto Sans"
                font.pixelSize: 12
                elide: Text.ElideMiddle
            }

            Btn {
                id: undoDeleteButton
                objectName: "undoDeleteButton"
                Accessible.name:  qsTr("Undo delete")
                visible: studio.trashPending
                text: qsTr("Undo")
                width: Math.max(56, implicitWidth)
                height: 24
                font.pixelSize: 11
                onClicked: {
                    studio.undoDelete()
                    if (screen.item && screen.item.refreshDocs)
                        screen.item.refreshDocs()
                }
            }
        }

        Timer {
            interval: studio.trashPending ? 6000 : 2400
            running: studio.status !== ""
            onTriggered: studio.clearStatus()
            onIntervalChanged: if (running) restart()
        }
    }

    ShortcutsOverlay {
        id: shortcutsOverlay
    }

    // ---- shortcuts (skipped while a text field or modal dialog has focus)
    Shortcut {
        sequences: ["?"]
        enabled: !studio.typing && !leaveDialog.visible && !(screen.item && screen.item.exportOpen === true)
        onActivated: shortcutsOverlay.visible ? shortcutsOverlay.close() : shortcutsOverlay.open()
    }
    Shortcut {
        sequences: ["Q"]
        enabled: !studio.typing && !win.modalOpen
        onActivated: win.close()
    }
    Shortcut {
        sequences: ["Ctrl+Z"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: studio.undo()
    }
    Shortcut {
        sequences: ["Ctrl+Shift+Z", "Ctrl+Y"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: studio.redo()
    }
    Shortcut {
        sequences: ["Ctrl+S"]
        enabled: studio.editing && !win.modalOpen
        onActivated: {
            win.commitEditor()
            studio.saveWithFeedback()
        }
    }
    Shortcut {
        sequences: ["Ctrl+E"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: if (screen.item && screen.item.openExport) screen.item.openExport()
    }
    Shortcut {
        sequences: ["Ctrl+D"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: studio.duplicateSelected()
    }
    Shortcut {
        sequences: ["Ctrl+A"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: studio.selectAll()
    }
    Shortcut {
        sequences: ["Ctrl+0"]
        enabled: studio.editing && !win.modalOpen
        onActivated: if (screen.item && screen.item.canvasItem) screen.item.canvasItem.fitToView()
    }
    Shortcut {
        sequences: ["Ctrl+=", "Ctrl++"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: if (screen.item && screen.item.canvasItem) screen.item.canvasItem.zoomBy(1.2)
    }
    Shortcut {
        sequences: ["Ctrl+-"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: if (screen.item && screen.item.canvasItem) screen.item.canvasItem.zoomBy(1 / 1.2)
    }
    Shortcut {
        sequences: ["Ctrl+1", "["]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: if (screen.item && screen.item.toggleLeft) screen.item.toggleLeft()
    }
    Shortcut {
        sequences: ["Ctrl+2", "]"]
        enabled: studio.editing && !studio.typing && !win.modalOpen
        onActivated: if (screen.item && screen.item.toggleRight) screen.item.toggleRight()
    }
}

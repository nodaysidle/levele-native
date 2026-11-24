import QtQuick
import Levele 1.0
import "ui"

Item {
    id: root
    property alias canvas: view

    // The image tool needs a file dialog; the editor owns it.
    signal imageRequested()

    // The item has no size at Component.onCompleted, so keep fitting until the
    // layout settles — and stop as soon as the user pans or zooms by hand.
    function fitOnce() {
        if (view.width < 4 || view.height < 4 || view.userMoved)
            return
        view.fitToView()
    }

    // Text currently in the inline editor (committed on close).
    function editorText() { return editor.text }

    onWidthChanged: fitOnce()
    onHeightChanged: fitOnce()
    Component.onCompleted: fitOnce()

    CanvasItem {
        id: view
        objectName: "canvasStage"
        anchors.fill: parent
        focus: true
        document: studio
        canvasBg: theme.chrome
        borderColour: theme.border
        accentColour: theme.accent
        mutedColour: theme.muted

        onImageRequested: root.imageRequested()
    }

    Connections {
        target: studio
        function onDocumentReplaced() { view.fitToView() }
    }

    // The inline editor shares one notify signal for all its typography, so the
    // line height is re-applied when the value actually changes.
    Connections {
        target: view
        function onEditingChanged() {
            if (editor.visible) {
                if (editor.activeEditId !== view.editingId)
                    editor.loadTarget()
                else if (Math.abs(editor.lastApplied - view.editLineHeight) > 0.0001)
                    editor.applyLineHeight()
            }
        }
    }

    // Inline text editing, positioned over the element being edited.
    Rectangle {
        id: editBox
        visible: view.editingId !== ""
        x: view.editRect.x
        y: view.editRect.y
        width: Math.max(48, view.editRect.width)
        height: Math.max(24, view.editRect.height)
        color: theme.dark ? Qt.rgba(0, 0, 0, 0.18) : Qt.rgba(1, 1, 1, 0.28)
        border.width: 1
        border.color: theme.accent

        TextEdit {
            id: editor
            objectName: "inlineEditor"
            anchors.fill: parent
            anchors.margins: 1
            color: view.editColour
            selectionColor: theme.accent
            selectedTextColor: theme.accentInk
            font.family: "Noto Sans"
            font.pixelSize: Math.max(6, view.editFontPx)
            font.weight: view.editWeight
            font.letterSpacing: view.editSpacing
            horizontalAlignment: view.editAlign === "center" ? Text.AlignHCenter
                               : view.editAlign === "right" ? Text.AlignRight
                               : view.editAlign === "justify" ? Text.AlignJustify
                               : Text.AlignLeft
            verticalAlignment: view.editVAlign === "middle" ? Text.AlignVCenter
                             : view.editVAlign === "bottom" ? Text.AlignBottom
                             : Text.AlignTop
            wrapMode: TextEdit.Wrap
            selectByMouse: true
            textFormat: TextEdit.PlainText
            clip: true

            property real lastApplied: -1
            property bool applyingLineHeight: false
            property string activeEditId: ""

            // TextEdit has no line-height, so the element's own line height is
            // pushed into the document's block format (F25). Guard against
            // recursive lineCountChanged emissions triggered by mergeBlockFormat.
            function applyLineHeight() {
                if (applyingLineHeight || !visible || view.editingId === "")
                    return
                applyingLineHeight = true
                lastApplied = view.editLineHeight
                view.applyEditLineHeight(textDocument)
                applyingLineHeight = false
            }

            function loadTarget() {
                activeEditId = view.editingId
                text = view.editText
                forceActiveFocus()
                selectAll()
                applyLineHeight()
            }

            onVisibleChanged: {
                if (visible) {
                    loadTarget()
                } else {
                    lastApplied = -1
                    activeEditId = ""
                }
            }
            onTextChanged: applyLineHeight()
            onLineCountChanged: applyLineHeight()
            onActiveFocusChanged: {
                if (!activeFocus && visible)
                    view.commitEditText(text)
            }
            Keys.onEscapePressed: function(event) {
                view.cancelEditText()
                view.forceActiveFocus()
                event.accepted = true
            }
            Keys.onReturnPressed: function(event) {
                if (event.modifiers & Qt.ShiftModifier) {
                    event.accepted = false
                } else {
                    event.accepted = true
                    view.commitEditText(text)
                    view.forceActiveFocus()
                }
            }
        }
    }

    // Zoom read-out, bottom-right of the canvas area.
    Rectangle {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 16
        width: zoomRow.width + 16
        height: 30
        radius: theme.radius
        color: theme.surface
        border.color: theme.border
        border.width: 1

        Row {
            id: zoomRow
            anchors.centerIn: parent
            spacing: 6

            IconBtn {
                objectName: "zoomOutButton"
                width: 24
                height: 24
                glyph: "\u2212"
                glyphSize: 13
                tip: qsTr("Zoom out (Ctrl+-)")
                label: qsTr("Zoom out")
                onClicked: view.zoomBy(1 / 1.2)
            }
            Text {
                text: Math.round(view.zoom * 100) + "%"
                color: theme.fg
                font.family: "JetBrains Mono"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
                Accessible.name:  qsTr("Zoom ") + Math.round(view.zoom * 100) + " per cent"
            }
            IconBtn {
                objectName: "zoomInButton"
                width: 24
                height: 24
                glyph: "+"
                glyphSize: 13
                tip: qsTr("Zoom in (Ctrl+=)")
                label: qsTr("Zoom in")
                onClicked: view.zoomBy(1.2)
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "ui"

Item {
    id: ed
    property alias canvasItem: stage.canvas
    readonly property bool exportOpen: exportDialog.visible

    Keys.onEscapePressed: function(event) {
        if (studio.selectionCount > 0) {
            studio.clearSelection()
            stage.canvas.forceActiveFocus()
            event.accepted = true
        }
    }

    // Panels collapse by hand (Ctrl+1 / Ctrl+2, [ and ]) and the inspector
    // becomes a floating sheet in a narrow tiling column.
    property bool leftOpen: true
    property bool rightOpen: true
    readonly property bool compact: width < 1100

    Component.onCompleted: rightOpen = !compact
    onCompactChanged: rightOpen = !compact

    function toggleLeft() { leftOpen = !leftOpen }
    function toggleRight() { rightOpen = !rightOpen }

    readonly property var tools: [
        { k: "select", g: "\u2196", tip: qsTr("Select (V)"), name: qsTr("Select tool") },
        { k: "rect", g: "\u25AD", tip: qsTr("Rectangle (R)"), name: qsTr("Rectangle tool") },
        { k: "roundRect", g: "\u25A2", tip: qsTr("Rounded rectangle"), name: qsTr("Rounded rectangle tool") },
        { k: "ellipse", g: "\u25EF", tip: qsTr("Ellipse (O)"), name: qsTr("Ellipse tool") },
        { k: "triangle", g: "\u25B3", tip: qsTr("Triangle"), name: qsTr("Triangle tool") },
        { k: "star", g: "\u2605", tip: qsTr("Star"), name: qsTr("Star tool") },
        { k: "line", g: "\u2571", tip: qsTr("Line (L)"), name: qsTr("Line tool") },
        { k: "arrow", g: "\u2192", tip: qsTr("Arrow"), name: qsTr("Arrow tool") },
        { k: "text", g: "T", tip: qsTr("Text (T)"), name: qsTr("Text tool") },
        { k: "image", g: "\u25A3", tip: qsTr("Place an image\u2026"), name: qsTr("Place an image") }
    ]

    Rectangle {
        anchors.fill: parent
        color: theme.chrome
    }

    // ------------------------------------------------------------- top bar
    Rectangle {
        id: topBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 52
        color: theme.chrome

        Row {
            anchors.left: parent.left
            anchors.leftMargin: 12
            anchors.right: topRightRow.left
            anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            IconBtn {
                objectName: "backButton"
                width: 32
                height: 32
                glyph: "\u2190"
                tip: qsTr("Back to your designs")
                label: qsTr("Back")
                onClicked: studio.closeDoc()
            }

            TextField {
                id: titleField
                objectName: "titleField"
                width: Math.max(80, Math.min(ed.compact ? 160 : 320,
                                             topBar.width - topRightRow.width - (dimLabel.visible ? dimLabel.implicitWidth + 16 : 0) - 64))
                Accessible.name:  qsTr("Design title")
                Accessible.description: qsTr("Name of this design")
                text: studio.title
                color: theme.fg
                font.family: "Noto Sans"
                font.pixelSize: 14
                font.weight: Font.Medium
                leftPadding: 8
                rightPadding: 8
                background: Rectangle {
                    color: "transparent"
                    radius: Math.min(4, theme.radius)
                    border.width: titleField.activeFocus ? 2 : 0
                    border.color: theme.accent
                }
                onActiveFocusChanged: if (!activeFocus) studio.setTitle(text)
                onAccepted: {
                    studio.setTitle(text)
                    stage.canvas.forceActiveFocus()
                }
                Keys.onEscapePressed: function(event) {
                    text = studio.title
                    stage.canvas.forceActiveFocus()
                    event.accepted = true
                }
            }

            Text {
                id: dimLabel
                visible: topBar.width >= 740
                text: Math.round(studio.docWidth) + " \u00d7 " + Math.round(studio.docHeight)
                color: theme.muted
                font.family: "JetBrains Mono"
                font.pixelSize: 11
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Row {
            id: topRightRow
            anchors.right: parent.right
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            IconBtn {
                objectName: "undoButton"
                width: 32
                height: 32
                glyph: "\u21B6"
                tip: qsTr("Undo (Ctrl+Z)")
                label: qsTr("Undo")
                enabled: studio.canUndo
                onClicked: studio.undo()
            }
            IconBtn {
                objectName: "redoButton"
                width: 32
                height: 32
                glyph: "\u21B7"
                tip: qsTr("Redo (Ctrl+Shift+Z)")
                label: qsTr("Redo")
                enabled: studio.canRedo
                onClicked: studio.redo()
            }
            IconBtn {
                objectName: "panelsToggle"
                width: 32
                height: 32
                glyph: "\u25E7"
                tip: qsTr("Show or hide the tools and layers (Ctrl+1)")
                label: qsTr("Toggle the tool and layer panel")
                toggleable: true
                picked: ed.leftOpen
                glyphColor: ed.leftOpen ? theme.accentInk : theme.fg
                onClicked: ed.toggleLeft()
            }
            IconBtn {
                objectName: "inspectorToggle"
                width: 32
                height: 32
                glyph: "\u25E8"
                tip: qsTr("Show or hide the inspector (Ctrl+2)")
                label: qsTr("Toggle the inspector")
                toggleable: true
                picked: ed.rightOpen
                glyphColor: ed.rightOpen ? theme.accentInk : theme.fg
                onClicked: ed.toggleRight()
            }
            Btn {
                text: qsTr("Grid")
                width: 56
                tip: qsTr("Show the 50 px grid (G)")
                variant: stage.canvas.grid ? "accent" : "ghost"
                onClicked: stage.canvas.grid = !stage.canvas.grid
            }
            Btn {
                text: qsTr("Fit")
                width: 48
                tip: qsTr("Fit the page in the window (Ctrl+0)")
                variant: "ghost"
                onClicked: stage.canvas.fitToView()
            }
            Btn {
                text: qsTr("Export")
                width: 80
                tip: qsTr("Export this design (Ctrl+E)")
                variant: "accent"
                onClicked: exportDialog.open()
            }
        }

        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: theme.border
        }
    }

    // --------------------------------------------------------- left panel
    Rectangle {
        id: leftPanel
        anchors.left: parent.left
        anchors.top: topBar.bottom
        anchors.bottom: pageStrip.top
        width: ed.leftOpen ? 240 : 0
        visible: ed.leftOpen
        color: theme.surface

        Column {
            anchors.fill: parent

            Item {
                width: parent.width
                height: 108

                Grid {
                    x: 12
                    y: 12
                    columns: 5
                    spacing: 4

                    Repeater {
                        model: ed.tools
                        delegate: IconBtn {
                            objectName: "tool-" + modelData.k
                            width: 40
                            height: 40
                            glyph: modelData.g
                            glyphSize: modelData.k === "text" ? 15 : 14
                            tip: modelData.tip
                            label: modelData.name
                            toggleable: modelData.k !== "image"
                            picked: modelData.k !== "image" && stage.canvas.tool === modelData.k
                            glyphColor: picked ? theme.accentInk : theme.fg
                            cornerRadius: Math.min(8, theme.radius)
                            onClicked: {
                                if (modelData.k === "image")
                                    ed.pickImage("place")
                                else
                                    stage.canvas.tool = modelData.k
                            }
                        }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 1
                color: theme.border
            }

            LayersPanel {
                width: parent.width
                height: parent.height - 109
            }
        }

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: theme.border
        }
    }

    // --------------------------------------------------------- right panel
    Rectangle {
        id: rightPanel
        objectName: "inspectorPanel"
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: ed.compact ? undefined : pageStrip.top
        anchors.margins: ed.compact ? 8 : 0
        width: ed.rightOpen ? (ed.compact ? 300 : 288) : 0
        height: ed.compact
                ? Math.min(parent.height - topBar.height - pageStrip.height - 16, 560)
                : undefined
        visible: ed.rightOpen
        // Over the canvas, not beside it, when the window is narrow.
        z: ed.compact ? 4 : 0
        radius: ed.compact ? theme.radius : 0
        color: theme.surface
        border.width: ed.compact ? 1 : 0
        border.color: theme.border

        Rectangle {
            anchors.left: parent.left
            width: ed.compact ? 0 : 1
            height: parent.height
            color: theme.border
        }

        Inspector {
            id: inspector
            anchors.fill: parent
            anchors.topMargin: 0
            onPickImage: ed.pickImage("replace")
        }
    }

    // --------------------------------------------------------------- canvas
    CanvasStage {
        id: stage
        onImageRequested: ed.pickImage("place")
        anchors.left: leftPanel.right
        anchors.right: ed.compact ? parent.right : rightPanel.left
        anchors.top: topBar.bottom
        anchors.bottom: pageStrip.top
    }

    // ------------------------------------------------------------ pagestrip
    Rectangle {
        id: pageStrip
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 104
        color: theme.surface

        Rectangle {
            anchors.top: parent.top
            width: parent.width
            height: 1
            color: theme.border
        }

        PageStrip {
            anchors.fill: parent
        }
    }

    ExportDialog {
        id: exportDialog
    }

    function openExport() {
        exportDialog.open()
    }

    // Which image dialog is being opened decides what an accepted file does.
    property string imageMode: "place"
    property bool imagePickerPending: false
    property string imageDocId: ""
    property string imagePageId: ""
    property string imageReplaceId: ""

    function pickImage(mode) {
        if (portal.busy) {
            studio.note(qsTr("Finish the current file choice first"))
            return
        }
        imageMode = mode
        imageDocId = studio.docId
        imagePageId = studio.currentPageId
        imageReplaceId = mode === "replace" ? studio.sel.id : ""
        imagePickerPending = true
        if (!portal.openFile(mode === "replace" ? qsTr("Choose an image") : qsTr("Place an image")))
            imageDialog.open()
    }

    function acceptImage(file) {
        if (studio.docId !== imageDocId || studio.currentPageId !== imagePageId)
            return
        if (imageMode === "replace")
            studio.replaceImage(imageReplaceId, file)
        else
            studio.addImage(file, 0, 0)
    }

    // Pending text edits that only commit on focus loss.
    function commitEdits() {
        if (titleField.text !== studio.title)
            studio.setTitle(titleField.text)
        if (stage.canvas.editingId !== "")
            stage.canvas.commitEditText(stage.editorText())
        inspector.commitEdits()
    }

    FileDialog {
        id: imageDialog
        title: ed.imageMode === "replace" ? "Choose an image" : "Place an image"
        nameFilters: ["Images (*.png *.jpg *.jpeg *.webp *.bmp *.gif)", "All files (*)"]
        onAccepted: {
            ed.imagePickerPending = false
            ed.acceptImage(selectedFile)
        }
        onRejected: ed.imagePickerPending = false
    }

    Connections {
        target: portal
        function onOpenFinished(file, useFallback) {
            if (!ed.imagePickerPending)
                return
            if (useFallback) {
                imageDialog.open()
            } else {
                ed.imagePickerPending = false
                if (file.toString() !== "")
                    ed.acceptImage(file)
            }
        }
    }

    Connections {
        target: studio
        function onTitleChanged() {
            if (!titleField.activeFocus)
                titleField.text = studio.title
        }
    }
}

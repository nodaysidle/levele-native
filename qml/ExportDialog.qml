import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import "ui"

Dialog {
    id: dlg
    objectName: "exportDialog"
    parent: Overlay.overlay
    anchors.centerIn: Overlay.overlay
    modal: true
    width: Math.min(440, Overlay.overlay ? Overlay.overlay.width - 32 : 440)
    padding: 0
    closePolicy: choosingFile ? Popup.NoAutoClose : Popup.CloseOnEscape

    property string format: "png"
    property int pngScale: 2
    property int gifWidth: 720
    property int gifFps: 12
    property bool portalPending: false
    readonly property bool choosingFile: portalPending || saveDialog.visible
    property string exportDocId: ""
    property string exportPageId: ""

    function sameExportTarget() {
        return visible && studio.editing && studio.docId === exportDocId
               && (format !== "png" || studio.currentPageId === exportPageId)
    }

    function exportFile(file) {
        if (!sameExportTarget())
            return
        if (format === "png")
            studio.exportPng(file, studio.currentPage, pngScale)
        else if (format === "pdf")
            studio.exportPdf(file)
        else
            studio.exportGif(file, gifWidth, gifFps)
        close()
    }

    background: Rectangle {
        color: theme.surface
        radius: theme.radius
        border.width: 1
        border.color: theme.border
    }

    contentItem: Item {
        implicitWidth: dlg.width
        implicitHeight: body.implicitHeight + 40

        Column {
            id: body
            x: 20
            y: 20
            width: parent.width - 40
            spacing: 16
            enabled: !dlg.choosingFile

            Text {
                text: qsTr("Export")
                Accessible.name:  qsTr("Export")
                Accessible.role: Accessible.Heading
                color: theme.fg
                font.family: "Noto Sans"
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }

            Row {
                spacing: 6
                Repeater {
                    model: ["png", "pdf", "gif"]
                    delegate: Btn {
                        text: modelData.toUpperCase()
                        width: 76
                        checkable: true
                        checked: dlg.format === modelData
                        Accessible.name:  qsTr("Export as ") + modelData.toUpperCase()
                        Accessible.selected: dlg.format === modelData
                        variant: dlg.format === modelData ? "accent" : "default"
                        onClicked: dlg.format = modelData
                    }
                }
            }

            Column {
                width: parent.width
                spacing: 10
                visible: dlg.format === "png"

                Caption {
                    width: parent.width
                    text: qsTr("Current page, rasterised at a multiple of the document size.")
                    wrapMode: Text.WordWrap
                }
                Row {
                    spacing: 6
                    Caption { text: qsTr("Scale"); width: 46; anchors.verticalCenter: parent.verticalCenter }
                    Repeater {
                        model: [1, 2, 3, 4]
                        delegate: Btn {
                            text: modelData + "\u00D7"
                            width: 48
                            checkable: true
                            checked: dlg.pngScale === modelData
                            Accessible.name:  qsTr("Scale ") + modelData + " times"
                            Accessible.selected: dlg.pngScale === modelData
                            variant: dlg.pngScale === modelData ? "accent" : "default"
                            onClicked: dlg.pngScale = modelData
                        }
                    }
                }
                Caption {
                    color: theme.muted
                    font.family: "JetBrains Mono"
                    text: Math.round(studio.docWidth * dlg.pngScale) + " \u00d7 "
                          + Math.round(studio.docHeight * dlg.pngScale) + " px"
                }
            }

            Column {
                width: parent.width
                spacing: 10
                visible: dlg.format === "pdf"

                Caption {
                    width: parent.width
                    text: qsTr("All pages, one per sheet, sized in millimetres from the document pixels.")
                    wrapMode: Text.WordWrap
                }
                Caption {
                    color: theme.muted
                    font.family: "JetBrains Mono"
                    text: studio.pageCount + (studio.pageCount === 1 ? " page" : " pages")
                }
            }

            Column {
                width: parent.width
                spacing: 10
                visible: dlg.format === "gif"

                Caption {
                    width: parent.width
                    text: qsTr("All pages as animation frames, looping forever.")
                    wrapMode: Text.WordWrap
                }
                Row {
                    spacing: 10
                    Caption { text: qsTr("Width"); width: 46; anchors.verticalCenter: parent.verticalCenter }
                    NumField {
                        width: 76
                        min: 64
                        max: 2000
                        step: 20
                        label: qsTr("GIF width in pixels")
                        value: dlg.gifWidth
                        onEdited: function(v) { dlg.gifWidth = v }
                    }
                    Caption { text: qsTr("fps"); width: 26; anchors.verticalCenter: parent.verticalCenter }
                    NumField {
                        width: 60
                        min: 1
                        max: 50
                        label: qsTr("GIF frames per second")
                        value: dlg.gifFps
                        onEdited: function(v) { dlg.gifFps = v }
                    }
                }
            }

            Row {
                width: parent.width
                spacing: 8
                Item { width: parent.width - 212; height: 1 }
                Btn { text: qsTr("Cancel"); width: 88; Accessible.name:  qsTr("Cancel export"); onClicked: dlg.close() }
                Btn {
                    objectName: "exportChooseButton"
                    text: studio.exporting ? qsTr("Exporting\u2026") : qsTr("Export\u2026")
                    width: 108
                    Accessible.name: studio.exporting ? qsTr("Export in progress") : qsTr("Choose where to save")
                    variant: "accent"
                    enabled: !studio.exporting && !portal.busy
                    onClicked: {
                        dlg.contentItem.forceActiveFocus()
                        dlg.exportDocId = studio.docId
                        dlg.exportPageId = studio.currentPageId
                        dlg.portalPending = portal.saveFile(qsTr("Export to file"),
                                                           studio.suggestedExportName(dlg.format))
                        if (!dlg.portalPending)
                            saveDialog.open()
                    }
                }
            }
        }
    }

    Connections {
        target: portal
        function onSaveFinished(file, useFallback) {
            if (!dlg.portalPending)
                return
            dlg.portalPending = false
            if (useFallback && dlg.sameExportTarget())
                saveDialog.open()
            else if (file.toString() !== "")
                dlg.exportFile(file)
        }
    }

    FileDialog {
        id: saveDialog
        title: qsTr("Export to file")
        fileMode: FileDialog.SaveFile
        defaultSuffix: dlg.format
        currentFile: "file://" + studio.dataRoot() + "/" + studio.suggestedExportName(dlg.format)
        nameFilters: dlg.format === "png" ? ["PNG image (*.png)"]
                   : dlg.format === "pdf" ? ["PDF document (*.pdf)"]
                   : ["Animated GIF (*.gif)"]
        onAccepted: dlg.exportFile(selectedFile)
    }
}

import QtQuick
import QtQuick.Controls
import Levele 1.0
import "ui"

Item {
    id: dash

    property var docs: []
    property var trash: []

    function refreshDocs() {
        docs = studio.listDocs()
        trash = studio.listTrash()
    }

    Component.onCompleted: refreshDocs()

    Connections {
        target: studio
        function onTrashChanged() { dash.refreshDocs() }
    }

    Rectangle {
        anchors.fill: parent
        color: theme.chrome
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.height + 80
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: content
            x: 40
            y: 36
            width: dash.width - 80
            spacing: 24

            // ---- header
            Item {
                width: parent.width
                height: 72

                Column {
                    anchors.left: parent.left
                    anchors.right: newDesignBtn.left
                    anchors.rightMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Text {
                        width: parent.width
                        text: qsTr("Levele")
                        Accessible.name:  qsTr("Levele, design studio")
                        Accessible.role: Accessible.Heading
                        color: theme.fg
                        font.family: "Noto Sans"
                        font.pixelSize: 32
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.5
                        elide: Text.ElideRight
                    }
                    Text {
                        width: parent.width
                        text: qsTr("Design studio \u00b7 ") + theme.themeName
                        color: theme.muted
                        font.family: "Noto Sans"
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                Btn {
                    id: newDesignBtn
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("New design")
                    width: 120
                    height: 36
                    variant: "accent"
                    onClicked: studio.newDocument(1080, 1080)
                }
            }

            // ---- presets
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: qsTr("Start from a size")
                    color: theme.muted
                    font.family: "Noto Sans"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                Flow {
                    width: parent.width
                    spacing: 8

                    Repeater {
                        model: studio.presets()
                        delegate: AbstractButton {
                            id: presetChip
                            width: chipText.implicitWidth + 28
                            height: 36
                            focusPolicy: Qt.StrongFocus
                            hoverEnabled: true

                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.name + " " + modelData.label
                            Accessible.onPressAction: presetChip.clicked()

                            contentItem: Text {
                                id: chipText
                                text: modelData.name + "  " + modelData.label
                                color: theme.fg
                                font.family: "Noto Sans"
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }

                            background: Rectangle {
                                radius: theme.radius
                                color: presetChip.hovered ? theme.surfaceAlt : theme.surface
                                border.width: presetChip.activeFocus ? 2 : 1
                                border.color: presetChip.activeFocus || presetChip.hovered ? theme.accent : theme.border
                            }

                            onClicked: studio.newDocument(modelData.w, modelData.h)
                        }
                    }
                }
            }

            // ---- recent
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: qsTr("Recent")
                    color: theme.muted
                    font.family: "Noto Sans"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                Text {
                    width: parent.width
                    visible: dash.docs.length === 0
                    text: qsTr("Nothing saved yet. Start from a template to get going \u2014 every change autosaves.")
                    color: theme.muted
                    font.family: "Noto Sans"
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }

                Repeater {
                    model: dash.docs
                    delegate: Rectangle {
                        width: content.width
                        height: 68
                        radius: theme.radius
                        color: recentHover.containsMouse ? theme.surfaceAlt : theme.surface
                        border.width: 1
                        border.color: theme.border

                        Rectangle {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            width: 48
                            height: 48
                            radius: Math.min(4, theme.radius)
                            color: theme.bg
                            border.width: 1
                            border.color: theme.border
                            clip: true

                            CanvasItem {
                                anchors.fill: parent
                                anchors.margins: 2
                                preview: true
                                interactive: false
                                canvasBg: theme.bg
                                previewJson: studio.docJson(modelData.id)
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 70
                            anchors.right: recentMeta.left
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.title
                            color: theme.fg
                            font.family: "Noto Sans"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Text {
                            id: recentMeta
                            anchors.right: recentButtons.left
                            anchors.rightMargin: 16
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.min(implicitWidth, Math.max(80, parent.width - 360))
                            horizontalAlignment: Text.AlignRight
                            elide: Text.ElideRight
                            text: Math.round(modelData.w) + "\u00d7" + Math.round(modelData.h)
                                  + " \u00b7 " + modelData.pages + "p \u00b7 " + modelData.modified
                            color: theme.muted
                            font.family: "JetBrains Mono"
                            font.pixelSize: 11
                        }
                        Row {
                            id: recentButtons
                            anchors.right: parent.right
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 4
                            Btn { text: qsTr("Open"); label: qsTr("Open ") + modelData.title; height: 28; font.pixelSize: 12; tip: qsTr("Open this design"); onClicked: { if (!studio.openDoc(modelData.id)) dash.refreshDocs() } }
                            Btn { text: qsTr("Copy"); label: qsTr("Duplicate ") + modelData.title; height: 28; font.pixelSize: 12; tip: qsTr("Make a copy"); onClicked: { studio.duplicateDoc(modelData.id); dash.refreshDocs() } }
                            Btn { text: qsTr("Delete"); label: qsTr("Delete ") + modelData.title; height: 28; font.pixelSize: 12; variant: "danger"; tip: qsTr("Move to trash (Undo appears below)"); onClicked: studio.deleteDoc(modelData.id) }
                        }

                        MouseArea {
                            id: recentHover
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.NoButton
                        }
                    }
                }
            }

            // ---- trash
            Column {
                width: parent.width
                spacing: 8
                visible: dash.trash.length > 0

                Text {
                    text: qsTr("Trash")
                    color: theme.muted
                    font.family: "Noto Sans"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                Repeater {
                    model: dash.trash
                    delegate: Rectangle {
                        width: content.width
                        height: 44
                        radius: theme.radius
                        color: theme.surface
                        border.width: 1
                        border.color: theme.border

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 12
                            anchors.right: restoreButton.left
                            anchors.rightMargin: 12
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.title + (modelData.deleted ? " · " + modelData.deleted : "")
                            color: theme.fg
                            font.family: "Noto Sans"
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }

                        Btn {
                            id: restoreButton
                            objectName: "restoreTrashButton"
                            anchors.right: parent.right
                            anchors.rightMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Restore")
                            label: qsTr("Restore ") + modelData.title
                            height: 28
                            onClicked: studio.restoreDoc(modelData.id)
                        }
                    }
                }
            }

            // ---- templates
            Column {
                width: parent.width
                spacing: 12

                Text {
                    text: qsTr("Templates")
                    color: theme.muted
                    font.family: "Noto Sans"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }

                Grid {
                    id: grid
                    width: parent.width
                    columns: Math.max(2, Math.floor(width / 250))
                    spacing: 16

                    Repeater {
                        model: studio.templates()
                        delegate: AbstractButton {
                            id: card
                            width: (grid.width - (grid.columns - 1) * grid.spacing) / grid.columns
                            height: card.width * 0.78 + 56
                            focusPolicy: Qt.StrongFocus
                            hoverEnabled: true

                            Accessible.role: Accessible.Button
                            Accessible.name: modelData.name + ", " + modelData.category
                            Accessible.description: Math.round(modelData.w) + " by "
                                + Math.round(modelData.h)
                                + (modelData.pages > 1 ? ", " + modelData.pages + " pages" : "")
                            Accessible.onPressAction: card.clicked()

                            contentItem: Item {
                                Rectangle {
                                    anchors.fill: parent
                                    radius: theme.radius
                                    color: theme.surface
                                    border.width: card.activeFocus ? 2 : 1
                                    border.color: card.activeFocus || card.hovered ? theme.accent : theme.border
                                }

                                Item {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 12
                                    height: parent.height - 64
                                    clip: true

                                    CanvasItem {
                                        anchors.fill: parent
                                        preview: true
                                        interactive: false
                                        canvasBg: theme.surface
                                        previewJson: studio.templateJson(index)
                                    }
                                }

                                Column {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: parent.right
                                    anchors.rightMargin: 12
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 10
                                    spacing: 2

                                    Text {
                                        width: parent.width
                                        text: modelData.name
                                        color: theme.fg
                                        font.family: "Noto Sans"
                                        font.pixelSize: 12
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        width: parent.width
                                        text: modelData.category + " \u00b7 " + Math.round(modelData.w)
                                              + "\u00d7" + Math.round(modelData.h)
                                              + (modelData.pages > 1 ? " \u00b7 " + modelData.pages + "p" : "")
                                        color: theme.muted
                                        font.family: "JetBrains Mono"
                                        font.pixelSize: 11
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            onClicked: studio.newFromTemplate(index)
                        }
                    }
                }
            }
        }
    }
}

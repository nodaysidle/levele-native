import QtQuick
import QtQuick.Controls
import "ui"

Item {
    id: root
    implicitWidth: 240

    readonly property int rowHeight: 32
    property int dragFrom: -1
    property int hoverIndex: -1

    Column {
        anchors.fill: parent
        spacing: 0

        Item {
            width: parent.width
            height: 36
            Row {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Text {
                    text: qsTr("Layers")
                    color: theme.muted
                    font.family: "Noto Sans"
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
                Text {
                    text: list.count
                    color: theme.muted
                    font.family: "JetBrains Mono"
                    font.pixelSize: 11
                }
            }
        }

        ListView {
            id: list
            objectName: "layerList"
            width: parent.width
            height: Math.min(Math.max(0, parent.height - 36 - 44),
                             Math.max(0, list.count * root.rowHeight - (list.count > 0 ? 2 : 0)))
            clip: true
            model: studio.elements
            // Row 0 is the bottom-most element, so render bottom-up: the top
            // layer then reads at the top of the panel.
            verticalLayoutDirection: ListView.BottomToTop
            boundsBehavior: Flickable.StopAtBounds
            spacing: 2
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: row
                required property int index
                required property string elemId
                required property string elemName
                required property string elemGlyph
                required property string elemType
                required property bool elemVisible
                required property bool elemLocked
                required property bool elemSelected

                width: list.width - 8
                x: 4
                height: root.rowHeight - 2
                radius: Math.min(4, theme.radius)
                color: elemSelected ? theme.accentSoft : (hover.containsMouse ? theme.surfaceAlt : "transparent")
                border.width: elemSelected ? 1 : 0
                border.color: theme.accent

                // Tab reaches the layer list; Enter/Space selects the layer.
                activeFocusOnTab: true
                Accessible.role: Accessible.ListItem
                Accessible.name: elemName
                Accessible.description: elemType + (elemVisible ? "" : ", hidden")
                                      + (elemLocked ? ", locked" : "")
                Accessible.selected: elemSelected
                Accessible.onPressAction: studio.selectElement(elemId, false)
                Keys.onReturnPressed: studio.selectElement(elemId, false)
                Keys.onSpacePressed: studio.selectElement(elemId, false)
                Keys.onPressed: function(event) {
                    // Alt+↑/↓ reorders the layer from the keyboard. The panel is
                    // drawn top-layer-first, so "up" is a higher index.
                    if ((event.modifiers & Qt.AltModifier)
                        && (event.key === Qt.Key_Up || event.key === Qt.Key_Down)) {
                        const to = index + (event.key === Qt.Key_Up ? 1 : -1)
                        if (to >= 0 && to < list.count) {
                            studio.selectElement(elemId, false)
                            studio.reorderElement(index, to)
                        }
                        event.accepted = true
                    }
                }

                Rectangle {
                    visible: row.activeFocus
                    anchors.fill: parent
                    anchors.margins: -2
                    radius: Math.min(4, theme.radius) + 2
                    color: "transparent"
                    border.width: 2
                    border.color: theme.accent
                }

                MouseArea {
                    id: hover
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    acceptedButtons: Qt.LeftButton
                    // The ListView becomes flickable once the panel is full and
                    // would steal this drag before reorder could fire.
                    preventStealing: true

                    property real pressY: 0
                    property bool dragging: false

                    onPressed: function(mouse) {
                        pressY = mouse.y
                        root.dragFrom = index
                        studio.selectElement(row.elemId, (mouse.modifiers & Qt.ShiftModifier) !== 0)
                    }
                    onPositionChanged: function(mouse) {
                        if (pressed && Math.abs(mouse.y - pressY) > 6) {
                            dragging = true
                            const vy = list.mapFromItem(row, 0, mouse.y).y
                            root.hoverIndex = list.indexAt(10, vy + list.contentY)
                        }
                    }
                    onReleased: function(mouse) {
                        const fromIdx = index
                        const toIdx = root.hoverIndex
                        const move = dragging && toIdx >= 0 && toIdx !== fromIdx
                        dragging = false
                        root.dragFrom = -1
                        root.hoverIndex = -1
                        if (move)
                            studio.reorderElement(fromIdx, toIdx)
                    }
                    onDoubleClicked: {
                        nameEdit.cancelling = false
                        nameEdit.text = row.elemName
                        nameEdit.visible = true
                        nameEdit.forceActiveFocus()
                        nameEdit.selectAll()
                    }
                }

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Text {
                        text: row.elemGlyph
                        width: 14
                        color: row.elemSelected ? theme.accent : theme.muted
                        font.pixelSize: 12
                        horizontalAlignment: Text.AlignHCenter
                        anchors.verticalCenter: parent.verticalCenter
                    }
                    Text {
                        id: nameText
                        width: row.width - 90
                        text: row.elemName.replace(/\r?\n/g, " ")
                        color: row.elemLocked ? theme.muted : theme.fg
                        font.family: "Noto Sans"
                        font.pixelSize: 12
                        maximumLineCount: 1
                        elide: Text.ElideRight
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !nameEdit.visible
                    }
                    TextField {
                        id: nameEdit
                        property bool cancelling: false
                        width: row.width - 90
                        visible: false
                        text: row.elemName
                        Accessible.name:  qsTr("Layer name")
                        color: theme.fg
                        font.family: "Noto Sans"
                        font.pixelSize: 12
                        padding: 0
                        background: null
                        onEditingFinished: {
                            visible = false
                            if (cancelling) {
                                cancelling = false
                                return
                            }
                            studio.renameElement(row.elemId, text)
                        }
                        Keys.onEscapePressed: function(event) {
                            cancelling = true
                            text = row.elemName
                            visible = false
                            row.forceActiveFocus()
                            event.accepted = true
                        }
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2

                    IconBtn {
                        width: 24
                        height: 24
                        glyph: row.elemVisible ? "\u25C9" : "\u25CC"
                        glyphSize: 13
                        glyphColor: row.elemVisible ? theme.muted : theme.border
                        tip: row.elemVisible ? "Hide layer" : "Show layer"
                        label: row.elemVisible ? "Hide " + row.elemName : "Show " + row.elemName
                        onClicked: studio.setElementVisible(row.elemId, !row.elemVisible)
                    }
                    IconBtn {
                        width: 24
                        height: 24
                        glyph: row.elemLocked ? "\u25A0" : "\u25A1"
                        glyphSize: 11
                        toggleable: true
                        picked: row.elemLocked
                        glyphColor: row.elemLocked ? theme.accentInk : theme.muted
                        tip: row.elemLocked ? "Unlock layer" : "Lock layer"
                        label: row.elemLocked ? "Unlock " + row.elemName : "Lock " + row.elemName
                        onClicked: studio.setElementLocked(row.elemId, !row.elemLocked)
                    }
                }
            }
        }

        // Reorder + destructive actions for the selection.
        Row {
            width: parent.width
            height: 44
            spacing: 4
            leftPadding: 10

            Repeater {
                model: [
                    { glyph: "\u2912", tip: qsTr("Bring to front"), act: "front" },
                    { glyph: "\u2191", tip: qsTr("Raise"), act: "raise" },
                    { glyph: "\u2193", tip: qsTr("Lower"), act: "lower" },
                    { glyph: "\u2913", tip: qsTr("Send to back"), act: "back" }
                ]
                delegate: IconBtn {
                    width: 32
                    height: 28
                    anchors.verticalCenter: parent.verticalCenter
                    glyph: modelData.glyph
                    glyphSize: 13
                    tip: modelData.tip
                    label: modelData.tip
                    glyphColor: studio.selectionCount > 0 ? theme.fg : theme.border
                    enabled: studio.selectionCount > 0
                    onClicked: {
                        if (studio.selectionCount === 0)
                            return
                        if (modelData.act === "front") studio.frontSelected()
                        else if (modelData.act === "raise") studio.raiseSelected()
                        else if (modelData.act === "lower") studio.lowerSelected()
                        else studio.backSelected()
                    }
                }
            }
        }
    }

    // Drop indicator while dragging a layer.
    Rectangle {
        visible: root.dragFrom >= 0 && root.hoverIndex >= 0
        x: 6
        width: parent.width - 12
        height: 2
        color: theme.accent
        y: {
            if (!visible)
                return 0
            var item = list.itemAtIndex(root.hoverIndex)
            if (!item)
                return 0
            return list.y + item.y - list.contentY + item.height / 2
        }
    }
}

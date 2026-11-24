import QtQuick
import QtQuick.Controls
import Levele 1.0
import "ui"

// Page strip: a horizontal ListView so pages past the window width are
// reachable, with drag reorder, keyboard page handling and 24 px targets.
Item {
    id: root
    implicitHeight: 104

    property int dragFrom: -1
    property int dragTo: -1

    ListView {
        id: pages
        objectName: "pageListView"
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.right: actions.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        height: 88
        orientation: ListView.Horizontal
        spacing: 10
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: studio.pageCount
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        focus: true

        Keys.onLeftPressed: studio.setCurrentPage(Math.max(0, studio.currentPage - 1))
        Keys.onRightPressed: studio.setCurrentPage(Math.min(studio.pageCount - 1,
                                                          studio.currentPage + 1))
        Keys.onPressed: function(event) {
            // Ctrl+Shift+←/→ moves the current page instead of switching to it.
            if ((event.modifiers & Qt.ControlModifier) && (event.modifiers & Qt.ShiftModifier)) {
                if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                    const to = studio.currentPage + (event.key === Qt.Key_Left ? -1 : 1)
                    if (to >= 0 && to < studio.pageCount)
                        studio.movePage(studio.currentPage, to)
                    event.accepted = true
                }
            }
        }

        delegate: Item {
            id: pageCell
            required property int index
            width: 76
            height: 88

            activeFocusOnTab: true
            Accessible.role: Accessible.PageTab
            Accessible.name:  qsTr("Page ") + (index + 1)
            Accessible.selected: index === studio.currentPage
            Accessible.onPressAction: studio.setCurrentPage(index)
            Keys.onReturnPressed: studio.setCurrentPage(index)
            Keys.onSpacePressed: studio.setCurrentPage(index)

            Rectangle {
                id: chip
                width: 76
                height: 70
                radius: theme.radius
                color: theme.surface
                border.width: index === studio.currentPage ? 2 : 1
                border.color: index === studio.currentPage ? theme.accent : theme.border
                clip: true

                CanvasItem {
                    anchors.fill: parent
                    anchors.margins: 4
                    document: studio
                    preview: true
                    interactive: false
                    pageIndex: pageCell.index
                }

                IconBtn {
                    visible: pageCell.index === studio.currentPage && studio.pageCount > 1
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 2
                    width: 24
                    height: 24
                    glyph: "\u00D7"
                    glyphSize: 12
                    tip: qsTr("Delete page ") + (pageCell.index + 1)
                    label: qsTr("Delete page ") + (pageCell.index + 1)
                    onClicked: {
                        const n = pageCell.index + 1
                        studio.deletePage(pageCell.index)
                        studio.note(qsTr("Deleted page %1 \u00b7 Ctrl+Z restores it").arg(n))
                    }
                }

                MouseArea {
                    id: chipMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    // Inside a Flickable the ListView would otherwise steal the
                    // drag after a few pixels and reorder could never fire.
                    preventStealing: true

                    property real pressX: 0

                    onPressed: function(mouse) {
                        pressX = mouse.x
                        root.dragFrom = pageCell.index
                        pageCell.forceActiveFocus()
                    }
                    onPositionChanged: function(mouse) {
                        if (!pressed)
                            return
                        if (Math.abs(mouse.x - pressX) > 6)
                            root.dragTo = pages.indexAt(pageCell.x + mouse.x, 4)
                    }
                    onReleased: {
                        if (root.dragFrom >= 0 && root.dragTo >= 0 && root.dragTo !== root.dragFrom)
                            studio.movePage(root.dragFrom, root.dragTo)
                        else
                            studio.setCurrentPage(pageCell.index)
                        root.dragFrom = -1
                        root.dragTo = -1
                    }
                }
            }

            Rectangle {
                visible: pageCell.activeFocus
                anchors.fill: chip
                anchors.margins: -3
                radius: theme.radius + 3
                color: "transparent"
                border.width: 2
                border.color: theme.accent
            }

            Text {
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                text: index + 1
                color: index === studio.currentPage ? theme.fg : theme.muted
                font.family: "JetBrains Mono"
                font.pixelSize: 11
            }
        }
    }

    Row {
        id: actions
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        spacing: 8

        IconBtn {
            id: duplicatePageButton
            width: 44
            height: 76
            glyph: "\u29C9"
            glyphSize: 16
            tip: qsTr("Duplicate page ") + (studio.currentPage + 1)
            label: qsTr("Duplicate page")
            onClicked: studio.duplicatePage(studio.currentPage)
        }

        IconBtn {
            id: addPageButton
            width: 44
            height: 76
            glyph: "+"
            glyphSize: 18
            tip: qsTr("Add a page")
            label: qsTr("Add page")
            onClicked: studio.addPage()
        }
    }

    // Drop indicator while dragging a page.
    Rectangle {
        visible: root.dragFrom >= 0 && root.dragTo >= 0
        width: 2
        height: 76
        y: pages.y
        color: theme.accent
        x: {
            if (!visible)
                return 0
            const item = pages.itemAtIndex(root.dragTo)
            if (!item)
                return pages.x
            return pages.x + item.x - pages.contentX + (root.dragTo > root.dragFrom ? item.width : 0)
        }
    }
}

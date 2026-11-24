import QtQuick
import QtQuick.Controls
import "ui"

Item {
    id: root
    implicitWidth: 288

    readonly property bool hasSel: (studio.sel.count || 0) > 0
    readonly property bool primaryLocked: hasSel && !studio.sel.multi && sv("locked", false)

    function sv(k, d) {
        var m = studio.sel
        var x = m[k]
        return x === undefined ? d : x
    }

    // The editor owns the file dialog (portal or Qt fallback).
    signal pickImage()

    // Pending text edit in the content area (commits on focus loss otherwise).
    function commitEdits() {
        if (contentArea.text !== contentArea.boundText)
            studio.setSelProp("text", contentArea.text)
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: body.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            id: body
            x: 16
            y: 16
            width: root.width - 32
            spacing: 16

            // ---- header
            Column {
                width: parent.width
                spacing: 4
                Text {
                    width: parent.width
                    Accessible.name: root.hasSel ? (studio.sel.multi ? qsTr("%1 elements selected").arg(studio.sel.count) : root.sv("name", "")) : qsTr("Nothing selected")
                    text: root.hasSel ? (studio.sel.multi ? qsTr("%1 selected").arg(studio.sel.count) : root.sv("name", "")) : qsTr("Nothing selected")
                    color: theme.fg
                    font.family: "Noto Sans"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Row {
                    width: parent.width
                    visible: root.hasSel
                    spacing: 8
                    Text {
                        width: root.primaryLocked ? parent.width - unlockBtn.width - 8 : parent.width
                        anchors.verticalCenter: parent.verticalCenter
                        text: root.hasSel ? (studio.sel.multi ? qsTr("Multiple elements") : (root.sv("type", "") + (root.primaryLocked ? qsTr(" \u00b7 Locked") : ""))) : ""
                        color: root.primaryLocked ? theme.yellow : theme.muted
                        font.family: "JetBrains Mono"
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Btn {
                        id: unlockBtn
                        visible: root.primaryLocked
                        text: qsTr("Unlock")
                        label: qsTr("Unlock selected layer")
                        height: 24
                        font.pixelSize: 11
                        onClicked: studio.setElementLocked(root.sv("id", ""), false)
                    }
                }
            }

            // ---- transform
            Section {
                title: qsTr("Transform")
                visible: root.hasSel
                enabled: !root.primaryLocked

                Grid {
                    columns: 4
                    columnSpacing: 6
                    rowSpacing: 6

                    Caption { text: qsTr("X"); width: 12 }
                    NumField { objectName: "xNumField"; width: 66; label: qsTr("X position"); mixed: studio.sel.count > 1 && studio.selMixed("x"); value: root.sv("x", 0); onEdited: function(v) { studio.setSelProp("x", v) } }
                    Caption { text: qsTr("Y"); width: 12 }
                    NumField { width: 66; label: qsTr("Y position"); mixed: studio.sel.count > 1 && studio.selMixed("y"); value: root.sv("y", 0); onEdited: function(v) { studio.setSelProp("y", v) } }

                    Caption { text: qsTr("W"); width: 12 }
                    NumField { width: 66; min: root.sv("hasFill", true) ? 1 : -100000; label: qsTr("Width"); mixed: studio.sel.count > 1 && studio.selMixed("w"); value: root.sv("w", 0); onEdited: function(v) { studio.setSelProp("w", v) } }
                    Caption { text: qsTr("H"); width: 12 }
                    NumField { width: 66; min: root.sv("hasFill", true) ? 1 : -100000; label: qsTr("Height"); mixed: studio.sel.count > 1 && studio.selMixed("h"); value: root.sv("h", 0); onEdited: function(v) { studio.setSelProp("h", v) } }

                    Caption { text: qsTr("\u2220"); width: 12 }
                    NumField { width: 66; step: 5; min: -360; max: 360; label: qsTr("Rotation"); mixed: studio.sel.count > 1 && studio.selMixed("rotation"); value: root.sv("rotation", 0); onEdited: function(v) { studio.setSelProp("rotation", v) } }
                    Caption { text: qsTr("%"); width: 12 }
                    NumField { objectName: "opacityNumField"; width: 66; min: 0; max: 100; label: qsTr("Opacity per cent"); mixed: studio.sel.count > 1 && studio.selMixed("opacity"); value: Math.round(root.sv("opacity", 1) * 100); onEdited: function(v) { studio.setSelProp("opacity", v / 100) } }
                }
            }

            // ---- fill
            Section {
                title: qsTr("Fill")
                // Lines, arrows, text, and images ignore fill in the renderer.
                visible: root.hasSel && root.sv("hasFill", true)
                         && !root.sv("isText", false) && !root.sv("isImage", false)
                enabled: !root.primaryLocked

                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Colour"); width: 58; anchors.verticalCenter: parent.verticalCenter }
                    ColorChip {
                        width: 44
                        label: qsTr("Fill colour")
                        value: root.sv("fill", "#888888")
                        onPicked: function(c) { studio.setSelProp("fill", c) }
                    }
                }

                Toggle {
                    label: qsTr("Gradient")
                    checked: root.sv("gradient", false)
                    onToggled: studio.setSelProp("gradient", checked)
                }

                Column {
                    width: parent.width
                    spacing: 6
                    visible: root.sv("gradient", false)

                    Row {
                        width: parent.width
                        spacing: 8
                        Caption { text: qsTr("From"); width: 58; anchors.verticalCenter: parent.verticalCenter }
                        ColorChip { width: 44; label: qsTr("Gradient from"); value: root.sv("gradFrom", "#ff8a5c"); onPicked: function(c) { studio.setSelProp("gradFrom", c) } }
                        Caption { text: qsTr("To"); width: 16; anchors.verticalCenter: parent.verticalCenter }
                        ColorChip { width: 44; label: qsTr("Gradient to"); value: root.sv("gradTo", "#ff3d7f"); onPicked: function(c) { studio.setSelProp("gradTo", c) } }
                    }
                    Row {
                        width: parent.width
                        spacing: 8
                        Caption { text: qsTr("Angle"); width: 58; anchors.verticalCenter: parent.verticalCenter }
                        NumField { width: 66; step: 15; min: -360; max: 360; label: qsTr("Gradient angle"); value: root.sv("gradAngle", 45); onEdited: function(v) { studio.setSelProp("gradAngle", v) } }
                    }
                }

                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Radius"); width: 58; anchors.verticalCenter: parent.verticalCenter }
                    NumField { width: 66; min: 0; max: 2000; label: qsTr("Corner radius"); value: root.sv("radius", 0); onEdited: function(v) { studio.setSelProp("radius", v) } }
                }
            }

            // ---- stroke (shapes, lines, and arrows)
            Section {
                title: qsTr("Stroke")
                visible: root.hasSel && !root.sv("isText", false) && !root.sv("isImage", false)
                enabled: !root.primaryLocked

                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Colour"); width: 58; anchors.verticalCenter: parent.verticalCenter }
                    ColorChip { width: 44; label: qsTr("Stroke colour"); value: root.sv("stroke", "#000000"); onPicked: function(c) { studio.setSelProp("stroke", c) } }
                    NumField { width: 56; min: 0; max: 200; label: qsTr("Stroke width"); value: root.sv("strokeWidth", 0); onEdited: function(v) { studio.setSelProp("strokeWidth", v) } }
                }
            }

            // ---- text
            Section {
                title: qsTr("Text")
                visible: root.hasSel && root.sv("isText", false)
                enabled: !root.primaryLocked

                TextArea {
                    id: contentArea
                    Accessible.name:  qsTr("Text content")
                    property string boundText: root.sv("text", "")
                    width: parent.width
                    height: 76
                    color: theme.fg
                    font.family: "Noto Sans"
                    font.pixelSize: 12
                    wrapMode: TextArea.Wrap
                    selectByMouse: true
                    leftPadding: 8
                    topPadding: 8
                    onBoundTextChanged: if (!activeFocus) text = boundText
                    Component.onCompleted: text = boundText
                    onActiveFocusChanged: if (!activeFocus && text !== boundText) studio.setSelProp("text", text)
                    background: Rectangle {
                        radius: Math.min(4, theme.radius)
                        color: theme.bg
                        border.width: 1
                        border.color: contentArea.activeFocus ? theme.accent : theme.border
                    }
                }

                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Size"); width: 34; anchors.verticalCenter: parent.verticalCenter }
                    NumField { width: 58; min: 4; max: 800; step: 2; label: qsTr("Font size"); mixed: studio.sel.count > 1 && studio.selMixed("fontSize"); value: root.sv("fontSize", 48); onEdited: function(v) { studio.setSelProp("fontSize", v) } }
                    Caption { text: qsTr("Line"); width: 30; anchors.verticalCenter: parent.verticalCenter }
                    NumField { width: 54; decimals: 2; step: 0.05; min: 0.6; max: 4; label: qsTr("Line height"); value: root.sv("lineHeight", 1.25); onEdited: function(v) { studio.setSelProp("lineHeight", v) } }
                }

                Row {
                    width: parent.width
                    spacing: 6
                    Caption { text: qsTr("Track"); width: 34; anchors.verticalCenter: parent.verticalCenter }
                    NumField { width: 54; step: 0.5; decimals: 1; min: -20; max: 200; label: qsTr("Letter spacing"); value: root.sv("letterSpacing", 0); onEdited: function(v) { studio.setSelProp("letterSpacing", v) } }
                    Caption { text: qsTr("Weight"); width: 42; anchors.verticalCenter: parent.verticalCenter }
                    Combo {
                        id: weightCombo
                        controlWidth: 88
                        label: qsTr("Text weight")
                        model: ["Light", "Regular", "Medium", "SemiBold", "Bold", "Black"]
                        Component.onCompleted: sync()
                        function sync() {
                            var w = root.sv("fontWeight", 600)
                            currentIndex = w <= 300 ? 0 : w <= 400 ? 1 : w <= 500 ? 2 : w <= 600 ? 3 : w <= 700 ? 4 : 5
                        }
                        onActivated: function(i) {
                            var map = [300, 400, 500, 600, 700, 900]
                            studio.setSelProp("fontWeight", map[i])
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Colour"); width: 48; anchors.verticalCenter: parent.verticalCenter }
                    ColorChip { width: 44; label: qsTr("Text colour"); value: root.sv("textColor", "#111111"); onPicked: function(c) { studio.setSelProp("textColor", c) } }

                    Repeater {
                        model: [
                            { m: "left", n: qsTr("Align text left") },
                            { m: "center", n: qsTr("Align text centre") },
                            { m: "right", n: qsTr("Align text right") },
                            { m: "justify", n: qsTr("Justify text") }
                        ]
                        delegate: AlignBtn {
                            width: 28
                            height: 28
                            anchors.verticalCenter: parent.verticalCenter
                            mode: modelData.m
                            picked: root.sv("textAlign", "left") === modelData.m
                            label: modelData.n
                            onClicked: studio.setSelProp("textAlign", modelData.m)
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: 4
                    Caption { text: qsTr("Vertical"); width: 48; anchors.verticalCenter: parent.verticalCenter }
                    Repeater {
                        model: [
                            { m: "top", g: "\u21E7", n: qsTr("Align text to the top") },
                            { m: "middle", g: "\u2195", n: qsTr("Centre text vertically") },
                            { m: "bottom", g: "\u21E9", n: qsTr("Align text to the bottom") }
                        ]
                        delegate: IconBtn {
                            width: 28
                            height: 28
                            glyph: modelData.g
                            glyphSize: 13
                            toggleable: true
                            picked: root.sv("textVAlign", "top") === modelData.m
                            glyphColor: picked ? theme.accentInk : theme.fg
                            tip: modelData.n
                            label: modelData.n
                            onClicked: studio.setSelProp("textVAlign", modelData.m)
                        }
                    }
                }
            }

            // ---- image
            Section {
                title: qsTr("Image")
                visible: root.hasSel && root.sv("isImage", false)
                enabled: !root.primaryLocked

                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Fit"); width: 24; anchors.verticalCenter: parent.verticalCenter }
                    Combo {
                        id: fitCombo
                        controlWidth: 110
                        label: qsTr("Image fit")
                        model: ["cover", "contain"]
                        Component.onCompleted: sync()
                        function sync() { currentIndex = root.sv("imageFit", "cover") === "contain" ? 1 : 0 }
                        onActivated: function(i) { studio.setSelProp("imageFit", i === 1 ? "contain" : "cover") }
                    }
                }
                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Radius"); width: 58; anchors.verticalCenter: parent.verticalCenter }
                    NumField { width: 66; min: 0; max: 2000; label: qsTr("Corner radius"); value: root.sv("radius", 0); onEdited: function(v) { studio.setSelProp("radius", v) } }
                }

                Btn {
                    width: parent.width
                    text: qsTr("Replace image\u2026")
                    tip: qsTr("Swap the image file, keeping its size and position")
                    onClicked: root.pickImage()
                }
            }

            // ---- arrange
            Section {
                title: qsTr("Arrange")
                visible: root.hasSel

                Row {
                    width: parent.width
                    spacing: 4
                    Repeater {
                        model: [
                            { g: "\u21E4", m: "left", n: qsTr("Align left") },
                            { g: "\u2194", m: "hcenter", n: qsTr("Align horizontal centre") },
                            { g: "\u21E5", m: "right", n: qsTr("Align right") },
                            { g: "\u2912", m: "top", n: qsTr("Align top") },
                            { g: "\u2195", m: "vcenter", n: qsTr("Align vertical centre") },
                            { g: "\u2913", m: "bottom", n: qsTr("Align bottom") }
                        ]
                        delegate: IconBtn {
                            width: 32
                            height: 28
                            glyph: modelData.g
                            glyphSize: 12
                            tip: modelData.n
                            label: modelData.n
                            enabled: !root.primaryLocked
                            onClicked: studio.alignSelected(modelData.m)
                        }
                    }
                }

                Row {
                    width: parent.width
                    spacing: 8
                    Btn { text: qsTr("Distribute H"); label: qsTr("Distribute horizontally"); font.pixelSize: 12; height: 28; enabled: !root.primaryLocked && studio.selectionCount > 2; onClicked: studio.distributeSelected("h") }
                    Btn { text: qsTr("Distribute V"); label: qsTr("Distribute vertically"); font.pixelSize: 12; height: 28; enabled: !root.primaryLocked && studio.selectionCount > 2; onClicked: studio.distributeSelected("v") }
                }

                Row {
                    width: parent.width
                    spacing: 8
                    Btn { text: qsTr("Duplicate"); height: 28; font.pixelSize: 12; onClicked: studio.duplicateSelected() }
                    Btn { text: qsTr("Delete"); height: 28; font.pixelSize: 12; variant: "danger"; enabled: !root.primaryLocked; onClicked: studio.deleteSelected() }
                }
            }

            // ---- page
            Section {
                title: qsTr("Page")

                Row {
                    width: parent.width
                    spacing: 8
                    Caption { text: qsTr("Background"); width: 76; anchors.verticalCenter: parent.verticalCenter }
                    ColorChip {
                        width: 44
                        label: qsTr("Page background")
                        value: studio.pageBackground
                        onPicked: function(c) { studio.setPageBackground(String(c)) }
                    }
                    Caption {
                        width: Math.max(40, parent.width - 136)
                        Accessible.name:  qsTr("Page size ") + Math.round(studio.docWidth) + " by "
                                         + Math.round(studio.docHeight) + " pixels"
                        text: Math.round(studio.docWidth) + " \u00d7 " + Math.round(studio.docHeight)
                        color: theme.muted
                        font.family: "JetBrains Mono"
                        font.pixelSize: 11
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }
    }

    Connections {
        target: studio
        function onSelectionChanged() {
            weightCombo.sync()
            fitCombo.sync()
        }
    }
}

import QtQuick
import QtQuick.Controls

// Numeric field that only pushes a value when the user commits, so typing a
// partial number never fights the live selection binding.
Item {
    id: root
    property real value: 0
    property real step: 1
    property real min: -100000
    property real max: 100000
    property int decimals: 0
    property string label: ""   // Accessible.name for screen readers
    property bool mixed: false  // selection disagrees: show "Mixed"

    signal edited(real v)

    implicitWidth: 76
    implicitHeight: 30

    function fmt(v) {
        return decimals > 0 ? v.toFixed(decimals) : String(Math.round(v))
    }

    function commit() {
        var v = parseFloat(String(field.text).replace(",", "."))
        if (isNaN(v)) {
            field.text = root.mixed ? "" : fmt(root.value)
            return
        }
        v = Math.max(root.min, Math.min(root.max, v))
        field.text = fmt(v)
        if (root.mixed || Math.abs(v - root.value) > 0.0001)
            root.edited(v)
    }

    function bump(delta) {
        var v = Math.max(root.min, Math.min(root.max, root.value + delta))
        field.text = fmt(v)
        root.edited(v)
    }

    function refresh() {
        if (field.activeFocus)
            return
        field.text = root.mixed ? "" : fmt(root.value)
    }

    // Read `value` directly here: a cached display property may not have been
    // re-evaluated yet when this fires.
    onValueChanged: refresh()
    onMixedChanged: refresh()
    Component.onCompleted: refresh()

    TextField {
        id: field
        objectName: "numericInput"
        anchors.fill: parent
        Accessible.name: root.label
        Accessible.description: root.label
        color: theme.fg
        font.family: "JetBrains Mono"
        font.pixelSize: 12
        selectByMouse: true
        verticalAlignment: TextInput.AlignVCenter
        leftPadding: 8
        rightPadding: 8
        topPadding: 0
        bottomPadding: 0
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        placeholderText: root.mixed ? "Mixed" : ""
        onEditingFinished: root.commit()
        onActiveFocusChanged: if (!activeFocus) root.commit()
        Keys.onUpPressed: function(event) { root.bump(root.step); event.accepted = true }
        Keys.onDownPressed: function(event) { root.bump(-root.step); event.accepted = true }
        Keys.onEscapePressed: function(event) {
            field.text = root.mixed ? "" : root.fmt(root.value)
            field.focus = false
            event.accepted = true
        }

        background: Rectangle {
            radius: Math.min(4, theme.radius)
            color: theme.bg
            border.width: field.activeFocus ? 2 : 1
            border.color: field.activeFocus ? theme.accent : theme.border
        }
    }
}

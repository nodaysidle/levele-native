import QtQuick

// Small caption / field label.
Text {
    property bool strong: false
    color: strong ? theme.fg : theme.muted
    font.family: "Noto Sans"
    font.pixelSize: 11
    font.weight: strong ? Font.Medium : Font.Normal
    verticalAlignment: Text.AlignVCenter
    elide: Text.ElideRight
}

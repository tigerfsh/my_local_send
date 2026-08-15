import QtQuick
import LocalSend

// Collapsed state: a portal (teleport gate) icon.
Image {
    id: bar
    signal clicked

    source: "qrc:/icons/portal.png"
    smooth: true
    fillMode: Image.PreserveAspectFit

    MouseArea {
        anchors.fill: parent
        onClicked: bar.clicked()
    }
}

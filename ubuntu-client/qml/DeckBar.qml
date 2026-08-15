import QtQuick
import QtQuick.Layouts
import LocalSend

// Horizontal "deck" state: up to 3 file covers piled with >80% overlap and
// up/down interleave, plus a count badge. Width is set by Main.qml to fit.
Rectangle {
    id: deck
    signal clicked

    color: "#F7F8FA"
    radius: 12
    border.color: "#D8DCE4"
    border.width: 1

    MouseArea {
        anchors.fill: parent
        onClicked: deck.clicked()
    }

    // Tightly overlapping pile of file covers.
    Item {
        id: pile
        anchors.left: parent.left
        anchors.leftMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: pileW
        height: 68

        property int shown: Math.max(0, Math.min(app.fileCount, 3))
        property int pileW: shown > 0 ? 44 + (shown - 1) * 8 : 0

        Repeater {
            model: fileModel
            delegate: Rectangle {
                id: card
                visible: index < 3
                x: index * 8                    // ~82% overlap
                y: (index % 2 === 0) ? 0 : 24   // up/down interleave
                width: 44
                height: 44
                radius: 8
                border.color: "#FFFFFF"
                border.width: 2
                color: model.isImage ? "#DCEBFF" : (model.isVideo ? "#FFE7D9" : "#E8EAF0")

                property string thumb: app.thumbUrl(model.fileId, model.cachePath, model.fileType)

                Image {
                    anchors.fill: parent
                    anchors.margins: 2
                    source: card.thumb
                    visible: card.thumb !== ""
                    fillMode: Image.PreserveAspectCrop
                    clip: true
                }
                Text {
                    visible: card.thumb === ""
                    text: model.isVideo ? "▶" : "▤"
                    anchors.centerIn: parent
                    color: "#5A6B8C"
                    font.pixelSize: 16
                }
            }
        }
    }

    // File-count badge.
    Rectangle {
        id: countBadge
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 46
        height: 46
        radius: 23
        color: "#2B3A67"
        border.color: "#FFFFFF"
        border.width: 2

        Text {
            anchors.centerIn: parent
            text: app.fileCount > 99 ? "99+" : app.fileCount
            color: "white"
            font.pixelSize: 15
            font.bold: true
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: item

    property bool isChecked: app.selectedIds.includes(model.fileId)
    signal previewRequested(string path, string type)

    implicitHeight: 54
    width: parent ? parent.width : 0

    Rectangle {
        anchors.fill: parent
        color: item.isChecked ? "#E8EDFB" : "transparent"
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        spacing: 8

        CheckBox {
            visible: app.multiSelect
            checked: item.isChecked
            onToggled: app.toggleSelect(model.fileId)
            Layout.alignment: Qt.AlignVCenter
        }

        // Thumbnail or type icon
        Rectangle {
            width: 36
            height: 36
            radius: 6
            color: model.isImage ? "#DCEBFF" : (model.isVideo ? "#FFE7D9" : "#E8EAF0")
            Layout.alignment: Qt.AlignVCenter

            Image {
                visible: model.isImage
                source: model.fileUrl
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectCrop
                clip: true
            }
            Text {
                visible: !model.isImage
                text: model.isVideo ? "▶" : "▤"
                color: "#5A6B8C"
                font.pixelSize: 16
                anchors.centerIn: parent
            }
        }

        Column {
            width: 130
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            Text {
                text: model.fileName
                width: parent.width
                elide: Text.ElideMiddle
                font.pixelSize: 13
                color: "#22304A"
            }
            Text {
                text: model.fileSizeText
                font.pixelSize: 11
                color: "#8A93A6"
            }
        }

        Item { Layout.fillWidth: true }

        Button {
            visible: model.isImage || model.isVideo
            text: model.isVideo ? "播放" : "预览"
            flat: true
            font.pixelSize: 12
            Layout.alignment: Qt.AlignVCenter
            onClicked: item.previewRequested(model.cachePath, model.isVideo ? "video" : "image")
        }

        Button {
            visible: !app.multiSelect
            text: "✕"
            flat: true
            font.pixelSize: 12
            Layout.alignment: Qt.AlignVCenter
            onClicked: app.deleteFile(model.fileId)
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: item

    property bool isChecked: app.selectedIds.includes(model.fileId)
    signal previewRequested(string path, string type)
    signal textPreviewRequested(string name, string content)

    // Drag the file(s) out to the desktop / a directory. In multi-select mode
    // this exports all selected files, otherwise just this file.
    Drag.dragType: Drag.Automatic
    Drag.supportedActions: Qt.CopyAction
    Drag.mimeData: { "text/uri-list": app.dragUriList(model.fileId) }
    Drag.active: dragHandler.active
    Drag.onDragFinished: (action) => {
        if (action === Qt.CopyAction) console.log("[drag] exported files")
    }

    DragHandler {
        id: dragHandler
    }

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
            id: thumbBox
            width: 36
            height: 36
            radius: 6
            color: model.isImage ? "#DCEBFF" : (model.isVideo ? "#FFE7D9" : "#E8EAF0")
            Layout.alignment: Qt.AlignVCenter

            property string thumb: app.thumbUrl(model.fileId, model.cachePath, model.fileType)

            Image {
                visible: thumbBox.thumb !== ""
                source: thumbBox.thumb
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectCrop
                clip: true
            }
            Text {
                visible: thumbBox.thumb === ""
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
            onClicked: item.previewRequested(model.fileUrl, model.isVideo ? "video" : "image")
        }

        // Doc / other files: text preview in-app, or open with the system app.
        Button {
            visible: !model.isImage && !model.isVideo
            text: "打开"
            flat: true
            font.pixelSize: 12
            Layout.alignment: Qt.AlignVCenter
            onClicked: {
                var content = app.readTextFile(model.cachePath)
                if (content !== "") {
                    item.textPreviewRequested(model.fileName, content)
                } else {
                    app.openWithSystem(model.cachePath)
                }
            }
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

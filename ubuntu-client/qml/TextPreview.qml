import QtQuick
import QtQuick.Controls

// In-app preview for text files.
Popup {
    id: dlg
    anchors.centerIn: parent
    width: 340
    height: 460
    padding: 16
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string fileName: ""
    property string content: ""

    contentItem: Column {
        spacing: 12

        Text {
            text: dlg.fileName
            font.pixelSize: 15
            font.bold: true
            color: "#2B3A67"
            elide: Text.ElideMiddle
            width: parent.width
        }

        ScrollView {
            width: parent.width
            height: 360
            clip: true

            Text {
                text: dlg.content
                textFormat: Text.PlainText
                wrapMode: Text.WrapAnywhere
                font.family: "monospace"
                font.pixelSize: 12
                color: "#22304A"
                width: parent.width
            }
        }

        Button {
            text: "关闭"
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: dlg.close()
        }
    }
}

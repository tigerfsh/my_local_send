import QtQuick
import QtQuick.Layouts
import LocalSend

Rectangle {
    id: bar
    signal clicked

    color: "#2B3A67"
    radius: 12
    border.color: "#1E2948"
    border.width: 1

    MouseArea {
        anchors.fill: parent
        onClicked: bar.clicked()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: 10
        spacing: 8

        Text {
            text: "中转站"
            color: "#FFFFFF"
            font.pixelSize: 14
            font.bold: true
            Layout.alignment: Qt.AlignVCenter
        }

        Item { Layout.fillWidth: true }

        Text {
            text: "⋮"
            color: "#9FB0E0"
            font.pixelSize: 16
            Layout.alignment: Qt.AlignVCenter
        }
    }

    // Red file-count badge (top-right)
    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: -6
        anchors.rightMargin: -6
        width: 20
        height: 20
        radius: 10
        color: "#E5484D"
        visible: app.fileCount > 0

        Text {
            anchors.centerIn: parent
            text: app.fileCount > 99 ? "99+" : app.fileCount
            color: "white"
            font.pixelSize: 10
            font.bold: true
        }
    }
}

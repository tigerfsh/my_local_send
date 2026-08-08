import QtQuick

Rectangle {
    id: preview
    property string path: ""
    signal close()

    color: "#E8000000"
    radius: 12
    visible: false

    Image {
        anchors.fill: parent
        anchors.margins: 16
        source: preview.path
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Text {
        text: "点击任意位置关闭预览"
        color: "#CCCCCC"
        font.pixelSize: 12
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 8
        anchors.horizontalCenter: parent.horizontalCenter
    }

    MouseArea {
        anchors.fill: parent
        onClicked: preview.close()
    }
}

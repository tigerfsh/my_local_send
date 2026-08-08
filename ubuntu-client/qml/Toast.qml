import QtQuick

Rectangle {
    id: toast

    property string toastText: ""
    visible: false
    z: 99

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: 12

    width: textItem.implicitWidth + 32
    height: 34
    radius: 17
    color: "#E02B3A67"

    Text {
        id: textItem
        anchors.centerIn: parent
        text: toast.toastText
        color: "white"
        font.pixelSize: 12
    }

    function show(message) {
        toast.toastText = message
        toast.visible = true
        hideTimer.restart()
    }

    Timer {
        id: hideTimer
        interval: 2200
        onTriggered: toast.visible = false
    }
}

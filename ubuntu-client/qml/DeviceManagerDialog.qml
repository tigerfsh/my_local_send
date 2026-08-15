import QtQuick
import QtQuick.Controls
import LocalSend

// Device manager: shows devices auto-discovered on the LAN (UDP multicast).
// The user picks a device and taps "配对". No manual IP entry needed.
Popup {
    id: dlg
    anchors.centerIn: parent
    width: 336
    height: 460
    padding: 16
    modal: true

    contentItem: Column {
        spacing: 12

        Text {
            text: "设备管理"
            font.pixelSize: 16
            font.bold: true
            color: "#2B3A67"
        }

        Text {
            text: "同一局域网的设备会自动出现在下方"
            font.pixelSize: 11
            color: "#8A93A6"
        }

        ListView {
            id: deviceList
            width: parent.width
            height: 300
            clip: true
            model: deviceModel
            spacing: 4

            delegate: Rectangle {
                width: deviceList.width
                height: 44
                radius: 6
                color: "#F2F4F8"

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 8

                    Text {
                        text: deviceName
                        font.pixelSize: 13
                        color: "#22304A"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: isOnline ? "● 在线" : "○ 离线"
                        font.pixelSize: 11
                        color: isOnline ? "#2E9E5B" : "#9AA3B5"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: ip + ":" + tcpPort
                        font.pixelSize: 11
                        color: "#8A93A6"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Item { width: 6 }

                    Button {
                        text: isTrusted ? "解除信任" : "配对"
                        font.pixelSize: 11
                        flat: true
                        anchors.verticalCenter: parent.verticalCenter
                        onClicked: isTrusted ? app.removeDevice(deviceId) : app.requestPair(deviceId)
                    }
                }
            }
        }

        // Empty state hint.
        Text {
            visible: deviceList.count === 0
            text: "正在扫描局域网设备…"
            font.pixelSize: 12
            color: "#8A93A6"
            anchors.horizontalCenter: parent.horizontalCenter
        }

        Row {
            spacing: 10
            anchors.horizontalCenter: parent.horizontalCenter

            Button {
                text: "重新扫描"
                onClicked: app.rescan()
            }
            Button {
                text: "关闭"
                onClicked: dlg.close()
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import LocalSend

Popup {
    id: dlg
    anchors.centerIn: parent
    width: 360
    height: 420
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

        ListView {
            id: deviceList
            width: parent.width
            height: 230
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

        Row {
            spacing: 8
            TextField {
                id: ipInput
                width: 150
                placeholderText: "设备 IP"
            }
            TextField {
                id: portInput
                width: 70
                placeholderText: "端口"
                text: "53318"
                validator: IntValidator { bottom: 1; top: 65535 }
            }
            Button {
                text: "手动连接"
                onClicked: app.connectByIp(ipInput.text.trim(), parseInt(portInput.text))
            }
        }

        Text {
            text: "本机: %1  (设备ID: %2)".arg(app.localIp).arg(app.selfDeviceId)
            font.pixelSize: 11
            color: "#8A93A6"
            wrapMode: Text.WordWrap
        }

        Button {
            text: "关闭"
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: dlg.close()
        }
    }
}

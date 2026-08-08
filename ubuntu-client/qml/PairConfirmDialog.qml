import QtQuick
import QtQuick.Controls

Popup {
    id: dlg

    property string deviceId: ""
    property string deviceName: ""

    anchors.centerIn: parent
    width: 300
    padding: 16
    modal: true

    contentItem: Column {
        spacing: 12

        Text {
            text: "配对请求"
            font.pixelSize: 16
            font.bold: true
            color: "#2B3A67"
        }

        Text {
            text: "设备「%1」请求与本机配对，是否接受？".arg(dlg.deviceName)
            font.pixelSize: 13
            color: "#44506B"
            wrapMode: Text.WordWrap
        }

        Row {
            spacing: 12
            Button {
                text: "拒绝"
                onClicked: {
                    app.respondPair(dlg.deviceId, false)
                    dlg.close()
                }
            }
            Button {
                text: "接受"
                highlighted: true
                onClicked: {
                    app.respondPair(dlg.deviceId, true)
                    dlg.close()
                }
            }
        }
    }
}

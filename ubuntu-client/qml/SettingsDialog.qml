import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs

Popup {
    id: dlg
    anchors.centerIn: parent
    width: 340
    padding: 16
    modal: true

    contentItem: Column {
        spacing: 14

        Text {
            text: "设置"
            font.pixelSize: 16
            font.bold: true
            color: "#2B3A67"
        }

        Row {
            spacing: 10
            Text {
                text: "暂存目录"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
            Text {
                text: app.cacheDir()
                font.pixelSize: 11
                color: "#8A93A6"
                elide: Text.ElideMiddle
                width: 150
                anchors.verticalCenter: parent.verticalCenter
            }
            Button {
                text: "选择"
                font.pixelSize: 12
                onClicked: folderDialog.open()
            }
        }

        Row {
            spacing: 10
            Text {
                text: "自动收起时长(毫秒)"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
            SpinBox {
                from: 2000
                to: 30000
                stepSize: 500
                value: app.autoCollapseMs
                onValueModified: app.autoCollapseMs = value
            }
        }

        Row {
            spacing: 10
            Text {
                text: "悬浮窗位置锁定"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
            Switch {
                checked: app.floatWindowLocked
                onToggled: app.floatWindowLocked = checked
            }
        }

        Row {
            spacing: 10
            Text {
                text: "缓存过期(小时，0=不过期)"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
            SpinBox {
                from: 0
                to: 720
                value: app.cacheExpireHours
                onValueModified: app.cacheExpireHours = value
            }
        }

        Row {
            spacing: 10
            Text {
                text: "传输限速(KB/s，0=不限)"
                font.pixelSize: 13
                anchors.verticalCenter: parent.verticalCenter
            }
            SpinBox {
                from: 0
                to: 1048576
                value: app.maxRateBps / 1024
                onValueModified: app.maxRateBps = value * 1024
            }
        }

        Button {
            text: "关闭"
            anchors.horizontalCenter: parent.horizontalCenter
            onClicked: dlg.close()
        }
    }

    FolderDialog {
        id: folderDialog
        title: "选择暂存目录"
        onAccepted: app.setCacheDir(folderDialog.selectedFolder)
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import LocalSend

Rectangle {
    id: panel

    property int panelState: 0
    property int maxListHeight: 380
    signal userAction()
    signal toggleFull()
    signal requestPreview(string path, string type)
    signal requestTextPreview(string name, string content)
    signal requestSettings()
    signal requestClose()
    signal requestDevices()

    color: "#F7F8FA"
    radius: 12
    border.color: "#D8DCE4"
    border.width: 1

    readonly property int headerH: 40
    readonly property int footerH: 44
    property int listH: panelState === 1
                       ? Math.min(maxListHeight, listView.contentHeight + 4)
                       : maxListHeight
    property int panelHeight: headerH + listH + footerH

    // Click on empty panel area: reset idle timer, toggle to full height.
    MouseArea {
        anchors.fill: parent
        z: 0
        onClicked: {
            panel.userAction()
            if (panel.panelState === 1) panel.toggleFull()
        }
    }

    Column {
        anchors.fill: parent

        // ----- Header -----
        Rectangle {
            width: parent.width
            height: panel.headerH
            color: "transparent"
            radius: 12

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 6
                spacing: 6

                Text {
                    text: "中转站"
                    font.pixelSize: 14
                    font.bold: true
                    color: "#2B3A67"
                    Layout.alignment: Qt.AlignVCenter
                }
                Item { Layout.fillWidth: true }

                Button {
                    text: "📌"
                    flat: true
                    font.pixelSize: 13
                    opacity: app.pinned ? 1.0 : 0.4
                    ToolTip.visible: hovered
                    ToolTip.text: app.pinned ? "取消钉住" : "钉住"
                    onClicked: app.pinned = !app.pinned
                }

                Button {
                    text: "⋮"
                    flat: true
                    onClicked: {
                        panel.userAction()
                        menu.popup()
                    }
                    Menu {
                        id: menu
                        MenuItem {
                            text: "多选"
                            onTriggered: { app.multiSelect = true; panel.userAction() }
                        }
                        MenuItem {
                            text: "设备管理"
                            onTriggered: { panel.requestDevices(); panel.userAction() }
                        }
                        MenuItem {
                            text: "设置"
                            onTriggered: { panel.requestSettings(); panel.userAction() }
                        }
                        MenuItem {
                            text: "关闭"
                            enabled: !app.pinned
                            onTriggered: { panel.requestClose(); panel.userAction() }
                        }
                    }
                }
            }
        }

        // ----- File list -----
        ListView {
            id: listView
            width: parent.width
            height: panel.listH
            clip: true
            model: fileModel
            currentIndex: -1

            delegate: FileListItem {
                width: listView.width
                onPreviewRequested: (path, type) => panel.requestPreview(path, type)
                onTextPreviewRequested: (name, content) => panel.requestTextPreview(name, content)
            }

            DropArea {
                anchors.fill: parent
                onEntered: (drag) => {
                    console.log("[drop] list entered urls=", drag.urls.length)
                    if (drag.urls.length > 0) {
                        drag.accept()
                        panel.userAction()
                    }
                }
                onDropped: (drop) => {
                    console.log("[drop] list dropped urls=", drop.urls.length)
                    if (drop.urls.length > 0) app.addFiles(drop.urls)
                }
            }
        }

        // ----- Footer -----
        Rectangle {
            width: parent.width
            height: panel.footerH
            color: "transparent"

            Row {
                anchors.centerIn: parent
                spacing: 8

                Button {
                    visible: !app.multiSelect
                    text: "＋ 添加文件"
                    onClicked: {
                        panel.userAction()
                        fileDialog.open()
                    }
                }

                Button {
                    visible: app.multiSelect
                    text: "全选"
                    onClicked: app.selectAll()
                }

                Button {
                    visible: app.multiSelect
                    text: "批量删除 (%1)".arg(app.selectedIds.length)
                    highlighted: true
                    enabled: app.selectedIds.length > 0
                    onClicked: {
                        panel.userAction()
                        confirmDelete.open()
                    }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: "选择要存入中转站的文件"
        fileMode: FileDialog.OpenFiles
        onAccepted: app.addFiles(selectedFiles)
    }

    Dialog {
        id: confirmDelete
        title: "批量删除"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.NoButton
        implicitWidth: 300
        implicitHeight: 150
        contentItem: Column {
            spacing: 12
            Text { text: "确定删除选中的 %1 个文件？将同步到所有可信设备。".arg(app.selectedIds.length) }
            Row {
                spacing: 12
                Button { text: "取消"; onClicked: confirmDelete.close() }
                Button {
                    text: "确认删除"
                    highlighted: true
                    onClicked: {
                        app.deleteSelected()
                        confirmDelete.close()
                    }
                }
            }
        }
    }
}

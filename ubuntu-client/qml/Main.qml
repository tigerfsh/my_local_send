import QtQuick
import QtQuick.Window
import QtQuick.Controls
import LocalSend

Window {
    id: root
    visible: true
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"

    property int panelState: 0          // 0 collapsed, 1 auto height, 2 full height
    property bool previewOpen: false
    property string previewPath: ""
    property string previewType: ""     // image | video

    readonly property int collapsedW: 148
    readonly property int collapsedH: 46
    readonly property int expandedW: 344
    property int maxListHeight: 380

    x: Math.max(0, Screen.desktopAvailableWidth - width - 24)
    y: 90

    width: previewOpen
           ? Math.min(840, Screen.desktopAvailableWidth * 0.72)
           : (panelState === 0 ? collapsedW : expandedW)
    height: previewOpen
            ? Math.min(600, Screen.desktopAvailableHeight * 0.72)
            : (panelState === 0 ? collapsedH : panelContent.panelHeight)

    Behavior on width { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }
    Behavior on height { NumberAnimation { duration: 200; easing.type: Easing.InOutQuad } }

    function collapse() {
        if (root.panelState !== 0) root.panelState = 0
    }
    function expand() {
        if (root.panelState === 0) root.panelState = 1
        root.resetTimer()
    }
    function resetTimer() { collapseTimer.restart() }

    Timer {
        id: collapseTimer
        interval: Math.max(2000, app.autoCollapseMs)
        repeat: false
        running: root.panelState !== 0
        onTriggered: root.panelState = 0
    }

    // Global drop target: expand on drag enter, accept file drops anywhere.
    DropArea {
        anchors.fill: parent
        onEntered: (drag) => { if (drag.urls.length > 0) root.expand() }
        onDropped: (drop) => { if (drop.urls.length > 0) app.addFiles(drop.urls) }
    }

    Loader {
        id: panelLoader
        anchors.fill: parent
        visible: !root.previewOpen
        sourceComponent: root.panelState === 0 ? collapsedBar : expandedPanel
    }

    Component {
        id: collapsedBar
        CollapsedBar {
            anchors.fill: parent
            onClicked: root.expand()
        }
    }

    Component {
        id: expandedPanel
        ExpandedPanel {
            id: panelContent
            anchors.fill: parent
            panelState: root.panelState
            maxListHeight: root.maxListHeight
            onUserAction: root.resetTimer()
            onToggleFull: root.panelState = 2
            onRequestPreview: (path, type) => {
                root.previewPath = path
                root.previewType = type
                root.previewOpen = true
            }
            onRequestSettings: settingsDialog.open()
            onRequestClose: root.collapse()
            onRequestDevices: deviceDialog.open()
        }
    }

    ImagePreview {
        visible: root.previewOpen && root.previewType === "image"
        path: root.previewPath
        onClose: root.previewOpen = false
    }
    VideoPreview {
        visible: root.previewOpen && root.previewType === "video"
        path: root.previewPath
        onClose: root.previewOpen = false
    }

    SettingsDialog { id: settingsDialog }
    DeviceManagerDialog { id: deviceDialog }
    PairConfirmDialog { id: pairDialog }
    Toast { id: toast }

    Connections {
        target: app
        function onPairRequested(deviceId, deviceName) {
            pairDialog.deviceId = deviceId
            pairDialog.deviceName = deviceName
            pairDialog.open()
        }
        function onToast(message) { toast.show(message) }
        function onErrorOccurred(message) { toast.show(message) }
    }
}

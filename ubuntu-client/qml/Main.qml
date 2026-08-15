import QtQuick
import QtQuick.Window
import QtQuick.Controls
import LocalSend

Window {
    id: root
    visible: true
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
    color: "transparent"

    // 0 collapsed (narrow strip)
    // 1 horizontal deck (stacked thumbnails + count)
    // 2 vertical list
    property int panelState: 0
    property bool hovering: false       // mouse is over the (masked) window
    property bool previewOpen: false
    property string previewPath: ""
    property string previewType: ""     // image | video

    readonly property int collapsedW: 40
    readonly property int collapsedH: 84   // matches the expanded deck height
    readonly property int expandedW: 344
    readonly property int deckH: 84
    readonly property int panelHeaderH: 40
    readonly property int panelFooterH: 44
    property int maxListHeight: 380
    readonly property int expandedMaxH: panelHeaderH + maxListHeight + panelFooterH

    // The window keeps its expanded geometry at all times. State changes are
    // done purely with a window mask (no resize), which avoids flicker from the
    // compositor animating a resize of a frameless transparent X11 window.
    x: Math.max(0, Screen.virtualX + Screen.width - width)
    y: app.windowY

    width: previewOpen
           ? Math.min(840, Screen.desktopAvailableWidth * 0.72)
           : expandedW
    height: previewOpen
            ? Math.min(600, Screen.desktopAvailableHeight * 0.72)
            : expandedMaxH

    // Deck width fits the pile + badge so there is no dead space on the right.
    function computeDeckWidth() {
        var n = Math.max(0, Math.min(app.fileCount, 3))
        var pileW = n > 0 ? 44 + (n - 1) * 8 : 0
        return Math.min(expandedW, 16 + pileW + 12 + 46 + 16)
    }

    function updateMask() {
        if (root.previewOpen) {
            app.clearWindowMask()
        } else if (root.panelState === 0) {
            app.setWindowMask(expandedW - collapsedW, 0, collapsedW, collapsedH)
        } else if (root.panelState === 1) {
            var dw = root.computeDeckWidth()
            app.setWindowMask(expandedW - dw, 0, dw, deckH)
        } else {
            app.setWindowMask(0, 0, expandedW, panelContent.panelHeight)
        }
    }

    onPanelStateChanged: {
        console.log("[ui] panelState=", root.panelState)
        root.updateMask()
        // Leaving multi-select mode when the panel collapses back.
        if (root.panelState === 0) app.multiSelect = false
    }
    onPreviewOpenChanged: root.updateMask()
    onActiveChanged: {
        console.log("[ui] active=", root.active)
        if (!root.active && root.panelState !== 0 && !app.pinned) root.collapse()
    }
    Component.onCompleted: root.updateMask()

    function collapse() {
        if (root.panelState !== 0) root.panelState = 0
    }
    function expand() {
        if (root.panelState === 0) root.panelState = 1
    }

    // Dialogs are declared before the collapse timer so its `running` binding
    // can reference their `visible` state safely.
    SettingsDialog { id: settingsDialog }
    DeviceManagerDialog { id: deviceDialog }
    PairConfirmDialog { id: pairDialog }
    TextPreview { id: textPreview }

    // Auto-collapse only counts down while the pointer is OUTSIDE the window
    // and no sub-page/dialog is open.
    Timer {
        id: collapseTimer
        interval: Math.max(2000, app.autoCollapseMs)
        repeat: false
        running: root.panelState !== 0 && !root.hovering && !root.previewOpen
                 && !deviceDialog.visible && !settingsDialog.visible && !pairDialog.visible
                 && !app.pinned
        onTriggered: root.panelState = 0
    }

    // Hover detector (does not consume clicks/drags).
    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        onContainsMouseChanged: {
            root.hovering = hoverArea.containsMouse
            console.log("[ui] hovering=", root.hovering)
        }
    }

    // Global drop target: expand on drag enter, accept file drops anywhere.
    DropArea {
        anchors.fill: parent
        onEntered: (drag) => {
            console.log("[drop] entered urls=", drag.urls.length)
            if (drag.urls.length > 0) {
                drag.accept()
                root.expand()
            }
        }
        onDropped: (drop) => {
            console.log("[drop] dropped urls=", drop.urls.length, "text=", drop.text)
            if (drop.urls.length > 0) app.addFiles(drop.urls)
        }
    }

    CollapsedBar {
        id: collapsedItem
        x: expandedW - collapsedW
        y: 0
        width: collapsedW
        height: collapsedH
        visible: root.panelState === 0 && !root.previewOpen
        onClicked: root.expand()
    }

    DeckBar {
        id: deckItem
        x: expandedW - root.computeDeckWidth()
        y: 0
        width: root.computeDeckWidth()
        height: deckH
        visible: root.panelState === 1 && !root.previewOpen
        onClicked: root.panelState = 2
    }

    ExpandedPanel {
        id: panelContent
        x: 0
        y: 0
        width: expandedW
        height: panelHeight
        panelState: 2                       // always the full vertical list
        maxListHeight: root.maxListHeight
        visible: root.panelState === 2 && !root.previewOpen
        onPanelHeightChanged: {
            if (root.panelState === 2 && !root.previewOpen) root.updateMask()
        }
        onRequestPreview: (path, type) => {
            root.previewPath = path
            root.previewType = type
            root.previewOpen = true
        }
        onRequestSettings: settingsDialog.open()
        onRequestClose: root.collapse()
        onRequestDevices: deviceDialog.open()
        onRequestTextPreview: (name, content) => {
            textPreview.fileName = name
            textPreview.content = content
            textPreview.open()
        }
    }

    ImagePreview {
        anchors.fill: parent
        z: 10
        visible: root.previewOpen && root.previewType === "image"
        path: root.previewPath
        onClose: root.previewOpen = false
    }
    VideoPreview {
        anchors.fill: parent
        z: 10
        visible: root.previewOpen && root.previewType === "video"
        path: root.previewPath
        onClose: root.previewOpen = false
    }

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
        function onFilesChanged() {
            if (root.panelState === 1 && !root.previewOpen) root.updateMask()
        }
    }
}

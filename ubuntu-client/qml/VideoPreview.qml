import QtQuick
import QtQuick.Controls
import QtMultimedia

Rectangle {
    id: preview
    property string path: ""
    signal close()

    color: "#E8000000"
    radius: 12
    visible: false

    MediaPlayer {
        id: player
        source: preview.path
        audioOutput: AudioOutput {}
        videoOutput: videoOut
    }

    VideoOutput {
        id: videoOut
        anchors.fill: parent
        anchors.margins: 16
        fillMode: VideoOutput.PreserveAspectFit
    }

    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 12
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        Button {
            text: player.playbackState === MediaPlayer.PlayingState ? "暂停" : "播放"
            onClicked: player.playbackState === MediaPlayer.PlayingState ? player.pause() : player.play()
        }

        Button {
            text: "关闭"
            onClicked: preview.close()
        }
    }

    onVisibleChanged: {
        if (visible) player.play()
        else player.stop()
    }
    onPathChanged: { if (visible) player.play() }
}

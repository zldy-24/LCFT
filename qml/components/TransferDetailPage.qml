import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f5f6f7"
    property var transferInfo: null

    component SmallBlueButton: Button {
        id: smallButton
        Layout.preferredWidth: 120
        Layout.preferredHeight: 30
        Layout.alignment: Qt.AlignHCenter
        font.pixelSize: 12
        background: Rectangle {
            radius: 6
            color: smallButton.down ? "#bfdbfe" : "#dbeafe"
            border.color: "#93c5fd"
        }
        contentItem: Text {
            text: smallButton.text
            color: "#1d4ed8"
            font: smallButton.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    component InfoRow: RowLayout {
        property string label: ""
        property string value: ""
        visible: value.length > 0
        spacing: 16
        Text {
            Layout.preferredWidth: 86
            text: label
            color: "#6b7280"
            font.pixelSize: 13
            horizontalAlignment: Text.AlignRight
        }
        Text {
            Layout.fillWidth: true
            text: value
            color: "#111827"
            font.pixelSize: 14
            wrapMode: Text.WrapAnywhere
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 56, root.transferInfo ? 620 : 520)
        spacing: root.transferInfo ? 14 : 16

        ColumnLayout {
            visible: !root.transferInfo
            Layout.fillWidth: true
            spacing: 16

            Text {
                text: "\u4f20\u8f93\u76d1\u63a7"
                font.pixelSize: 28
                font.bold: true
                color: "#111827"
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "\u5f53\u524d\u4f20\u8f93\u4efb\u52a1\u4f1a\u663e\u793a\u5728\u5de6\u4fa7\uff0c\u63a5\u6536\u5b8c\u6210\u7684\u6587\u4ef6\u4f1a\u81ea\u52a8\u4fdd\u5b58\u5230\u5f53\u524d\u6587\u4ef6\u5e93\u3002"
                color: "#6b7280"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }
            SmallBlueButton { text: "\u5168\u90e8\u6682\u505c"; onClicked: networkManager.pauseAll() }
            SmallBlueButton { text: "\u5168\u90e8\u7ee7\u7eed"; onClicked: networkManager.resumeAll() }
        }

        ColumnLayout {
            visible: !!root.transferInfo
            Layout.fillWidth: true
            spacing: 12

            Text {
                Layout.fillWidth: true
                text: root.transferInfo ? root.transferInfo.fileName : ""
                color: "#111827"
                font.pixelSize: 26
                font.bold: true
                wrapMode: Text.WrapAnywhere
            }

            Text {
                text: root.transferInfo ? root.stateText(root.transferInfo.state) : ""
                color: root.stateColor(root.transferInfo ? root.transferInfo.state : "")
                font.pixelSize: 16
                font.bold: true
            }

            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: root.transferInfo ? Math.max(1, root.transferInfo.fileSize) : 1
                value: root.transferInfo ? root.transferInfo.bytesTransferred : 0
            }

            InfoRow { label: "\u4f20\u8f93\u8fdb\u5ea6"; value: root.transferInfo ? root.progressText(root.transferInfo) : "" }
            InfoRow { label: "\u6587\u4ef6\u5168\u540d"; value: root.transferInfo ? (root.transferInfo.fileName || "") : "" }
            InfoRow { label: "\u6587\u4ef6\u683c\u5f0f"; value: root.transferInfo ? root.fileFormat(root.transferInfo.fileName) : "" }
            InfoRow { label: "\u6587\u4ef6\u5927\u5c0f"; value: root.transferInfo ? root.formatBytes(root.transferInfo.fileSize || 0) : "" }
            InfoRow { label: "\u4f20\u8f93\u65b9\u5411"; value: root.transferInfo ? root.directionText(root.transferInfo.direction) : "" }
            InfoRow { label: "\u4f20\u8f93\u901a\u9053"; value: root.transferInfo ? (root.transferInfo.isLan ? "\u5c40\u57df\u7f51" : "\u516c\u7f51") : "" }
            InfoRow {
                label: root.transferInfo && root.transferInfo.direction === "send"
                       ? "\u53d1\u7ed9"
                       : "\u6765\u81ea"
                value: root.transferInfo ? (root.transferInfo.peerName || "") : ""
            }
            InfoRow { label: "\u901f\u5ea6"; value: root.transferInfo ? (Number(root.transferInfo.speedMBps || 0).toFixed(2) + " MB/s") : "" }
            InfoRow {
                label: "\u7528\u65f6"
                value: root.transferInfo && root.transferInfo.state === "completed"
                       ? root.formatDuration(root.transferInfo.durationMs || 0)
                       : ""
            }
            InfoRow {
                label: "\u5931\u8d25\u539f\u56e0"
                value: root.transferInfo && root.transferInfo.state === "failed"
                       ? (root.transferInfo.errorMessage || "\u672a\u77e5\u9519\u8bef")
                       : ""
            }
        }
    }

    function stateText(state) {
        if (state === "sending") return "\u4f20\u8f93\u4e2d\uff08\u53d1\u9001\uff09"
        if (state === "receiving") return "\u4f20\u8f93\u4e2d\uff08\u63a5\u6536\uff09"
        if (state === "paused") return "\u5df2\u6682\u505c"
        if (state === "completed") return "\u4f20\u8f93\u6210\u529f"
        if (state === "failed") return "\u4f20\u8f93\u5931\u8d25"
        return state || ""
    }

    function stateColor(state) {
        if (state === "completed") return "#16a34a"
        if (state === "failed") return "#dc2626"
        if (state === "paused") return "#ca8a04"
        return "#2563eb"
    }

    function directionText(direction) {
        if (direction === "send") return "\u53d1\u9001"
        if (direction === "recv") return "\u63a5\u6536"
        return ""
    }

    function fileFormat(name) {
        var clean = name || ""
        var dot = clean.lastIndexOf(".")
        if (dot < 0 || dot === clean.length - 1)
            return "\u65e0\u6269\u5c55\u540d"
        return clean.slice(dot + 1).toLowerCase()
    }

    function formatBytes(bytes) {
        if (bytes >= 1024 * 1024 * 1024)
            return (bytes / 1024 / 1024 / 1024).toFixed(2) + " GB"
        if (bytes >= 1024 * 1024)
            return (bytes / 1024 / 1024).toFixed(2) + " MB"
        if (bytes >= 1024)
            return (bytes / 1024).toFixed(1) + " KB"
        return bytes + " B"
    }

    function progressText(item) {
        var total = Math.max(1, item.fileSize || 0)
        var transferred = Math.min(total, item.bytesTransferred || 0)
        var percent = total > 0 ? (transferred * 100 / total).toFixed(1) : "0.0"
        return formatBytes(transferred) + " / " + formatBytes(item.fileSize || 0) + " (" + percent + "%)"
    }

    function formatDuration(ms) {
        if (!ms || ms < 0)
            return "\u4e0d\u5230 1 \u79d2"
        var seconds = Math.max(1, Math.round(ms / 1000))
        var minutes = Math.floor(seconds / 60)
        var rest = seconds % 60
        if (minutes <= 0)
            return seconds + " \u79d2"
        return minutes + " \u5206 " + rest + " \u79d2"
    }
}

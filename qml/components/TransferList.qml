import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"
    property string searchText: ""
    property var filteredTransfers: {
        var text = searchText.trim().toLowerCase()
        var rows = []
        var items = networkManager.transfers
        for (var i = 0; i < items.length; ++i) {
            var item = items[i]
            if (text.length === 0 ||
                (item.fileName || "").toLowerCase().indexOf(text) >= 0 ||
                (item.peerName || "").toLowerCase().indexOf(text) >= 0 ||
                root.stateText(item.state).toLowerCase().indexOf(text) >= 0) {
                rows.push(item)
            }
        }
        return rows
    }

    component SmallBlueButton: Button {
        id: smallButton
        Layout.preferredWidth: 42
        Layout.preferredHeight: 24
        font.pixelSize: 11
        background: Rectangle {
            radius: 5
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

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "#f7f7f7"
            border.color: "#e5e7eb"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 12
                Text { text: "\u4f20\u8f93\u4efb\u52a1"; font.pixelSize: 20; font.bold: true; color: "#111827" }
                Item { Layout.fillWidth: true }
                SmallBlueButton { text: "\u6682\u505c"; onClicked: networkManager.pauseAll() }
                SmallBlueButton { text: "\u7ee7\u7eed"; onClicked: networkManager.resumeAll() }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.filteredTransfers
            delegate: ItemDelegate {
                width: list.width
                height: 82
                contentItem: Column {
                    spacing: 8
                    Text { text: modelData.fileName + "  [" + root.stateText(modelData.state) + "]"; font.bold: true; color: "#111827"; elide: Text.ElideRight; width: parent.width }
                    ProgressBar { width: parent.width; from: 0; to: Math.max(1, modelData.fileSize); value: modelData.bytesTransferred }
                    Text { text: (modelData.bytesTransferred / 1024 / 1024).toFixed(1) + " / " + (modelData.fileSize / 1024 / 1024).toFixed(1) + " MB, " + modelData.speedMBps.toFixed(2) + " MB/s"; color: "#6b7280"; font.pixelSize: 12 }
                }
            }
        }
    }

    function stateText(state) {
        if (state === "sending") return "\u53d1\u9001\u4e2d"
        if (state === "receiving") return "\u63a5\u6536\u4e2d"
        if (state === "paused") return "\u5df2\u6682\u505c"
        if (state === "completed") return "\u5df2\u5b8c\u6210"
        if (state === "failed") return "\u5931\u8d25"
        return state || ""
    }
}

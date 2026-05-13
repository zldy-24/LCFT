import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root
    modal: true
    focus: true
    anchors.centerIn: Overlay.overlay
    width: Math.min(420, Overlay.overlay ? Overlay.overlay.width - 32 : 420)
    closePolicy: Popup.NoAutoClose
    visible: networkManager.hasIncomingOffer

    background: Rectangle {
        radius: 18
        color: "white"
        border.color: "#e5e7eb"
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        Text {
            text: "\u6536\u5230\u6587\u4ef6\u8bf7\u6c42"
            font.pixelSize: 22
            font.bold: true
            color: "#111827"
        }

        Text {
            Layout.fillWidth: true
            text: networkManager.offerFrom + " \u60f3\u53d1\u9001 " + networkManager.offerFileName
            wrapMode: Text.WordWrap
            color: "#374151"
        }

        Text {
            text: "\u5927\u5c0f: " + Math.max(0, networkManager.offerFileSize / 1024 / 1024).toFixed(2) + " MB"
            color: "#6b7280"
        }

        RowLayout {
            Layout.fillWidth: true
            Button {
                Layout.fillWidth: true
                text: "\u62d2\u7edd"
                onClicked: networkManager.rejectOffer()
            }
            Button {
                Layout.fillWidth: true
                text: "\u63a5\u53d7"
                highlighted: true
                onClicked: networkManager.acceptOffer()
            }
        }
    }
}

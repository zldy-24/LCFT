import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    color: "#f5f6f7"

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

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 40, 520)
        spacing: 16

        Text { text: "\u4f20\u8f93\u76d1\u63a7"; font.pixelSize: 28; font.bold: true; color: "#111827"; Layout.alignment: Qt.AlignHCenter }
        Text { text: "\u5f53\u524d\u4f20\u8f93\u4efb\u52a1\u4f1a\u663e\u793a\u5728\u5de6\u4fa7\uff0c\u63a5\u6536\u5b8c\u6210\u7684\u6587\u4ef6\u4f1a\u81ea\u52a8\u4fdd\u5b58\u5230\u9ed8\u8ba4\u6587\u4ef6\u5e93\u3002"; color: "#6b7280"; wrapMode: Text.WordWrap; Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter }
        SmallBlueButton { text: "\u5168\u90e8\u6682\u505c"; onClicked: networkManager.pauseAll() }
        SmallBlueButton { text: "\u5168\u90e8\u7ee7\u7eed"; onClicked: networkManager.resumeAll() }
    }
}

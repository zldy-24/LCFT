import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"
    property var conversations: []
    property int currentIndex: 0
    signal conversationClicked(int index)

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: "#f7f7f7"
            border.color: "#e5e7eb"
            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 18
                text: "\u4f1a\u8bdd"
                font.pixelSize: 20
                font.bold: true
                color: "#111827"
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.conversations
            delegate: ItemDelegate {
                width: list.width
                height: 68
                highlighted: index === root.currentIndex
                onClicked: root.conversationClicked(index)
                contentItem: RowLayout {
                    spacing: 10
                    Rectangle {
                        Layout.preferredWidth: 10
                        Layout.preferredHeight: 10
                        radius: 5
                        color: modelData.isOnline ? "#22c55e" : "#9ca3af"
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Text { text: modelData.name || ""; font.pixelSize: 16; font.bold: true; color: "#111827"; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: modelData.lastMessage || ""; font.pixelSize: 13; color: "#6b7280"; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                }
            }
        }
    }
}

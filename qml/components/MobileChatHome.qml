import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"

    property var conversations: []
    property int currentIndex: 0
    property string searchText: ""
    signal conversationClicked(int index)
    signal openConversation()

    property var filteredConversations: {
        var text = searchText.trim().toLowerCase()
        var rows = []
        for (var i = 0; i < conversations.length; ++i) {
            var conv = conversations[i]
            if (text.length === 0 ||
                (conv.name || "").toLowerCase().indexOf(text) >= 0 ||
                (conv.lastMessage || "").toLowerCase().indexOf(text) >= 0) {
                var row = {}
                for (var key in conv)
                    row[key] = conv[key]
                row.sourceIndex = i
                rows.push(row)
            }
        }
        return rows
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: root.filteredConversations
        delegate: ItemDelegate {
            width: list.width
            height: 76
            onClicked: {
                root.conversationClicked(modelData.sourceIndex)
                root.openConversation()
            }
            contentItem: RowLayout {
                spacing: 12
                Rectangle {
                    Layout.preferredWidth: 52
                    Layout.preferredHeight: 52
                    radius: 26
                    color: index % 2 === 0 ? "#dbeafe" : "#dcfce7"
                    Text {
                        anchors.centerIn: parent
                        text: modelData.isSelf ? "\u6211" : (modelData.isOfficial ? "\ud83c\udf10" : (modelData.name || "?").slice(0, 1).toUpperCase())
                        color: "#111827"
                        font.pixelSize: modelData.isSelf ? 18 : 22
                        font.bold: true
                    }
                    Rectangle {
                        width: 12
                        height: 12
                        radius: 6
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        color: modelData.isOnline ? "#22c55e" : "#9ca3af"
                        border.color: "#ffffff"
                        border.width: 2
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text {
                        text: modelData.name || ""
                        color: "#111827"
                        font.pixelSize: 18
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: modelData.lastMessage || "\u6682\u65e0\u65b0\u6d88\u606f"
                        color: "#8a9099"
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }
            }
        }
    }

    Text {
        anchors.centerIn: parent
        visible: list.count === 0
        text: "\u6682\u65e0\u4f1a\u8bdd"
        color: "#9ca3af"
        font.pixelSize: 15
    }
}

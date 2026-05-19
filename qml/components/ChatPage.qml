import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f5f6f7"
    property var conversationInfo: null
    property bool showBackButton: false
    property bool canSendText: false
    property bool canSendFile: true
    signal openFilePicker()
    signal sendMessage(string text)
    signal backRequested()

    component ChatActionButton: Button {
        id: actionButton
        Layout.preferredWidth: 86
        Layout.preferredHeight: 34
        font.pixelSize: 14
        background: Rectangle {
            radius: 6
            color: actionButton.down ? "#d1d5db" : (actionButton.enabled ? "#e5e7eb" : "#f3f4f6")
            border.color: "#d1d5db"
        }
        contentItem: Text {
            text: actionButton.text
            color: actionButton.enabled ? "#111827" : "#9ca3af"
            font: actionButton.font
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
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                ToolButton {
                    visible: root.showBackButton
                    text: "<"
                    onClicked: root.backRequested()
                }
                Text { text: root.conversationInfo ? root.conversationInfo.name : "\u804a\u5929"; font.pixelSize: 20; font.bold: true; color: "#111827" }
                Item { Layout.fillWidth: true }
                Text { text: root.conversationInfo && root.conversationInfo.isOnline ? "\ud83d\udfe2 \u5728\u7ebf" : "\u26ab \u79bb\u7ebf"; color: "#6b7280" }
            }
        }

        ListView {
            id: messageList
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: 18
            spacing: 10
            clip: true
            model: root.conversationInfo && root.conversationInfo.messages ? root.conversationInfo.messages : []
            delegate: Item {
                width: messageList.width
                height: bubble.height + timeLabel.height + 8
                Rectangle {
                    id: bubble
                    width: modelData.kind === "file"
                           ? Math.min(parent.width * 0.74, 360)
                           : Math.min(parent.width * 0.72, msg.implicitWidth + 34)
                    height: contentColumn.implicitHeight + 18
                    radius: 14
                    color: modelData.sender === "me" ? "#dcfce7" : "#ffffff"
                    border.color: modelData.sender === "me" ? "#bbf7d0" : "#e5e7eb"
                    anchors.right: modelData.sender === "me" ? parent.right : undefined
                    anchors.left: modelData.sender === "other" ? parent.left : undefined

                    ColumnLayout {
                        id: contentColumn
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: modelData.kind === "file" ? 4 : 6

                        Text {
                            id: msg
                            Layout.fillWidth: true
                            text: modelData.kind === "file"
                                  ? ("\ud83d\udcc4 " + root.middleEllipsis(modelData.fileName || modelData.text || "", 28))
                                  : (modelData.text || "")
                            wrapMode: modelData.kind === "file" ? Text.NoWrap : Text.WrapAnywhere
                            elide: modelData.kind === "file" ? Text.ElideMiddle : Text.ElideNone
                            color: "#111827"
                            font.pixelSize: 15
                        }

                        Text {
                            visible: modelData.kind === "file"
                            Layout.fillWidth: true
                            text: root.fileDetailText(modelData)
                            color: "#6b7280"
                            font.pixelSize: 12
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Text {
                                text: ""
                                color: "#8a9099"
                                font.pixelSize: 10
                                visible: false
                            }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: modelData.kind === "file" ? root.formatBytes(modelData.fileSize || 0) : ""
                                color: "#8a9099"
                                font.pixelSize: 10
                                horizontalAlignment: Text.AlignRight
                                visible: modelData.kind === "file"
                            }
                        }
                    }
                }
                Text {
                    id: timeLabel
                    text: root.formatTime(modelData.timestamp || 0)
                    color: "#8a9099"
                    font.pixelSize: 10
                    anchors.top: bubble.bottom
                    anchors.topMargin: 2
                    anchors.right: modelData.sender === "me" ? bubble.right : undefined
                    anchors.left: modelData.sender === "other" ? bubble.left : undefined
                }
            }
            onCountChanged: Qt.callLater(positionViewAtEnd)
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            color: "white"
            border.color: "#e5e7eb"
            RowLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 12
                TextArea {
                    id: input
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    enabled: root.canSendText
                    placeholderText: root.canSendText ? "\u8f93\u5165\u6d88\u606f..." : "\u5f53\u524d\u4f1a\u8bdd\u4e0d\u53ef\u53d1\u9001\u6d88\u606f"
                    wrapMode: TextEdit.Wrap
                    Keys.onReturnPressed: function(event) {
                        if (!(event.modifiers & Qt.ShiftModifier) && input.text.trim().length > 0 && root.canSendText) {
                            root.sendMessage(input.text)
                            input.clear()
                            event.accepted = true
                        }
                    }
                }
                ColumnLayout {
                    ChatActionButton {
                        text: "\u6587\u4ef6"
                        enabled: root.canSendFile
                        onClicked: root.openFilePicker()
                    }
                    ChatActionButton {
                        text: "\u53d1\u9001"
                        enabled: input.text.trim().length > 0 && root.canSendText
                        onClicked: { root.sendMessage(input.text); input.clear() }
                    }
                }
            }
        }
    }

    function formatBytes(bytes) {
        if (bytes >= 1024 * 1024)
            return (bytes / 1024 / 1024).toFixed(2) + " MB"
        if (bytes >= 1024)
            return (bytes / 1024).toFixed(1) + " KB"
        return bytes + " B"
    }

    function middleEllipsis(name, maxLength) {
        if (!name || name.length <= maxLength)
            return name || ""
        var keep = Math.max(6, Math.floor((maxLength - 3) / 2))
        return name.slice(0, keep) + "..." + name.slice(name.length - keep)
    }

    function fileTypeLabel(name) {
        var clean = name || ""
        var dot = clean.lastIndexOf(".")
        if (dot >= 0 && dot < clean.length - 1)
            return clean.slice(dot + 1).toLowerCase() + "\u6587\u4ef6"
        return "\u6587\u4ef6"
    }

    function fileStatusText(status, sender) {
        if (status === "rejected") return "\u5df2\u62d2\u7edd"
        if (status === "sending") return sender === "me" ? "\u53d1\u9001\u4e2d" : "\u7b49\u5f85\u63a5\u6536"
        if (status === "receiving") return "\u63a5\u6536\u4e2d"
        if (status === "success") return sender === "me" ? "\u53d1\u9001\u6210\u529f" : "\u63a5\u6536\u6210\u529f"
        if (status === "failed") return sender === "me" ? "\u53d1\u9001\u5931\u8d25" : "\u63a5\u6536\u5931\u8d25"
        if (status === "paused") return "\u5df2\u6682\u505c"
        return ""
    }

    function transferChannelText(channel) {
        if (channel === "lan") return "\u901a\u8fc7\u5c40\u57df\u7f51\u4f20\u8f93"
        if (channel === "ecs") return "\u901a\u8fc7\u516c\u7f51\u4f20\u8f93"
        return ""
    }

    function fileDetailText(message) {
        var type = root.fileTypeLabel(message.fileName || message.text || "")
        var status = root.fileStatusText(message.fileStatus || "", message.sender || "")
        var channel = root.transferChannelText(message.transferChannel || "")
        if (status.length > 0 && channel.length > 0)
            return type + " / " + channel + " / " + status
        if (channel.length > 0)
            return type + " / " + channel
        return status.length > 0 ? type + " / " + status : type
    }

    function pad2(value) {
        return value < 10 ? "0" + value : "" + value
    }

    function formatTime(timestamp) {
        if (!timestamp)
            return ""
        var date = new Date(timestamp)
        var now = new Date()
        var startToday = new Date(now.getFullYear(), now.getMonth(), now.getDate()).getTime()
        var startMsgDay = new Date(date.getFullYear(), date.getMonth(), date.getDate()).getTime()
        var clock = pad2(date.getHours()) + ":" + pad2(date.getMinutes())
        if (startMsgDay === startToday)
            return clock
        if (startMsgDay === startToday - 24 * 60 * 60 * 1000)
            return "\u6628\u5929 " + clock
        return (date.getMonth() + 1) + "/" + date.getDate() + " " + clock
    }
}

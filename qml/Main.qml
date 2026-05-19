import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import "components"

ApplicationWindow {
    id: window
    width: 810
    height: 570
    visible: true
    title: "\u5c40\u57df\u7f51 / \u516c\u7f51\u6587\u4ef6\u4f20\u8f93"
    color: "#f5f6f7"

    property int currentTab: 0
    property int mobileTab: 0
    property int currentConversationIndex: 0
    property string currentConversationPeer: "LCFT"
    property string selectedPeerName: ""
    property bool selectedPeerIsLan: true
    property bool loggedIn: networkManager.connectionMode !== 0
    property bool mobileMode: Qt.platform.os === "android" || width < 760
    property string mobileSearchText: ""
    property bool choosingLibraryFile: false
    property string currentLibraryPath: networkManager.receivedFilesDir

    property var conversationData: {
        var convMap = {}
        convMap["LCFT"] = {
            name: "LCFT",
            lastMessage: "\u6b22\u8fce\u4f7f\u7528\u8de8\u5e73\u53f0\u6587\u4ef6\u4f20\u8f93\u5668\uff01",
            updatedAt: 1,
            isOfficial: true,
            isOnline: true,
            messages: [{ sender: "other", text: "\u6b22\u8fce\u4f7f\u7528\u8de8\u5e73\u53f0\u6587\u4ef6\u4f20\u8f93\u5668\uff01", timestamp: 1 }]
        }
        if ((networkManager.myName || "").length > 0) {
            convMap[networkManager.myName] = {
                name: networkManager.myName,
                lastMessage: "\u7ed9\u81ea\u5df1\u7684\u8bbe\u5907\u53d1\u9001\u6587\u4ef6",
                updatedAt: 2,
                isSelf: true,
                isOnline: true,
                messages: []
            }
        }
        var msgs = networkManager.chatMessages
        for (var i = 0; i < msgs.length; ++i) {
            var msg = msgs[i]
            var peer = msg.isMe ? (msg.to || "") : (msg.from || "")
            if (peer.length === 0)
                continue
            if (!convMap[peer]) {
                convMap[peer] = { name: peer, lastMessage: msg.isPlaceholder ? "\u6682\u65e0\u65b0\u6d88\u606f" : msg.text, updatedAt: msg.timestamp, messages: [] }
            }
            if (!convMap[peer].messages)
                convMap[peer].messages = []
            if (!msg.isPlaceholder) {
                convMap[peer].messages.push({
                    sender: msg.isMe ? "me" : "other",
                    text: msg.text,
                    timestamp: msg.timestamp,
                    statusUpdatedAt: msg.statusUpdatedAt || 0,
                    kind: msg.kind || "text",
                    fileName: msg.fileName || "",
                    fileSize: msg.fileSize || 0,
                    fileStatus: msg.fileStatus || "",
                    transferChannel: msg.transferChannel || ""
                })
            }
            var activityTime = Math.max(msg.timestamp || 0, msg.statusUpdatedAt || 0)
            if (activityTime >= convMap[peer].updatedAt) {
                convMap[peer].updatedAt = activityTime
                convMap[peer].lastMessage = msg.isPlaceholder ? "\u6682\u65e0\u65b0\u6d88\u606f" : conversationLastMessage(msg)
            }
        }
        var result = []
        for (var key in convMap) {
            if (convMap[key].isOnline === undefined)
                convMap[key].isOnline = isPeerOnline(convMap[key].name)
            result.push(convMap[key])
        }
        result.sort(function(a, b) {
            return b.updatedAt - a.updatedAt
        })
        return result
    }

    function fileStatusText(status, isMe) {
        if (status === "rejected") return "\u5df2\u62d2\u7edd"
        if (status === "sending") return isMe ? "\u53d1\u9001\u4e2d" : "\u7b49\u5f85\u63a5\u6536"
        if (status === "receiving") return "\u63a5\u6536\u4e2d"
        if (status === "success") return isMe ? "\u53d1\u9001\u6210\u529f" : "\u63a5\u6536\u6210\u529f"
        if (status === "failed") return isMe ? "\u53d1\u9001\u5931\u8d25" : "\u63a5\u6536\u5931\u8d25"
        if (status === "paused") return "\u5df2\u6682\u505c"
        return ""
    }

    function transferChannelText(channel) {
        if (channel === "lan") return "\u901a\u8fc7\u5c40\u57df\u7f51\u4f20\u8f93"
        if (channel === "ecs") return "\u901a\u8fc7\u516c\u7f51\u4f20\u8f93"
        return ""
    }

    function conversationLastMessage(msg) {
        if ((msg.kind || "") !== "file")
            return msg.text || ""
        var status = fileStatusText(msg.fileStatus || "", !!msg.isMe)
        var channel = transferChannelText(msg.transferChannel || "")
        var name = msg.fileName || msg.text || "\u6587\u4ef6"
        var prefix = status.length > 0 ? status : channel
        if (status.length > 0 && channel.length > 0)
            prefix = status + "\u00b7" + channel
        return prefix.length > 0 ? (prefix + ": " + name) : name
    }

    function isPeerOnline(peerName) {
        if (peerName === "LCFT" || peerName === networkManager.myName)
            return true
        var users = networkManager.onlineUsers
        for (var i = 0; i < users.length; ++i) {
            if (users[i].name === peerName)
                return true
        }
        var peers = networkManager.discoveredPeers
        for (var p = 0; p < peers.length; ++p) {
            if (peers[p].name === peerName)
                return true
        }
        return false
    }
    function sendMessageToCurrentConversation(text) {
        var content = text.trim()
        var peerName = currentConversationName()
        if (content.length === 0 || peerName.length === 0)
            return
        networkManager.ecsSendMessage(peerName, content)
    }

    function currentConversationName() {
        var index = conversationIndexByName(currentConversationPeer)
        if (index < 0)
            return ""
        return conversationData[index].name || ""
    }

    function conversationIndexByName(peerName) {
        for (var i = 0; i < conversationData.length; ++i) {
            if (conversationData[i].name === peerName)
                return i
        }
        return conversationData.length > 0 ? 0 : -1
    }

    function selectConversationByIndex(index) {
        if (index < 0 || index >= conversationData.length)
            return
        currentConversationPeer = conversationData[index].name || ""
        currentConversationIndex = index
    }

    function syncConversationSelection() {
        var index = conversationIndexByName(currentConversationPeer)
        if (index >= 0)
            currentConversationIndex = index
    }

    function canSendTextTo(peerName) {
        if (!peerName || peerName === "LCFT")
            return false
        if (peerName === networkManager.myName)
            return true
        return networkManager.ecsConnected
    }

    function isLanConversation(peerName) {
        if (!peerName || peerName === "LCFT")
            return false
        if (peerName === networkManager.myName)
            return true
        var peers = networkManager.discoveredPeers
        for (var i = 0; i < peers.length; ++i) {
            if (peers[i].name === peerName)
                return true
        }
        return networkManager.connectionMode === 2
    }

    function sendSelectedFile(url) {
        var path = networkManager.localPathFromUrl(url.toString())
        if (path.length === 0)
            return
        if ((currentTab === 1 || mobileTab === 1) && selectedPeerName.length > 0) {
            if (selectedPeerIsLan)
                networkManager.lanSendFile(selectedPeerName, path)
            else
                networkManager.ecsSendFile(selectedPeerName, path)
        } else {
            var peerName = currentConversationName()
            if (peerName.length === 0) {
                notify("\u8bf7\u5148\u9009\u62e9\u8981\u53d1\u9001\u7684\u8bbe\u5907")
                return
            }
            if (isLanConversation(peerName))
                networkManager.lanSendFile(peerName, path)
            else
                networkManager.ecsSendFile(peerName, path)
        }
    }

    function openConversation(peerName) {
        if (peerName.length === 0)
            return
        networkManager.ensureConversation(peerName)
        currentConversationPeer = peerName
        currentTab = 0
        mobileTab = 0
        Qt.callLater(function() {
            syncConversationSelection()
            if (window.mobileMode) {
                if (mobileChatStack.depth > 1)
                    mobileChatStack.replace(chatPageComp, {}, StackView.Immediate)
                else
                    mobileChatStack.push(chatPageComp, {}, StackView.Immediate)
            }
        })
    }

    function notify(message) {
        notifText.text = message
        notifBar.visible = true
        notifTimer.restart()
    }

    Connections {
        target: networkManager
        function onNotification(message) { window.notify(message) }
        function onTransferCompleted(taskId, fileName) { window.notify("\u4f20\u8f93\u5b8c\u6210: " + fileName) }
        function onTransferFailed(taskId, error) { window.notify("\u4f20\u8f93\u5931\u8d25: " + error) }
        function onChatMessagesChanged() { Qt.callLater(window.syncConversationSelection) }
        function onOnlineUsersChanged() { Qt.callLater(window.syncConversationSelection) }
        function onPeersChanged() { Qt.callLater(window.syncConversationSelection) }
    }

    Loader {
        anchors.fill: parent
        active: !loggedIn
        visible: active
        z: 10
        sourceComponent: LoginPage {}
    }

    FileDialog {
        id: nativeFileDialog
        title: "\u9009\u62e9\u6587\u4ef6"
        fileMode: FileDialog.OpenFile
        currentFolder: Qt.platform.os === "windows" ? "file:///" + networkManager.desktopDir().replace(/\\/g, "/") : ""
        onAccepted: window.sendSelectedFile(selectedFile)
    }

    Popup {
        id: fileSourcePopup
        modal: true
        focus: true
        width: 260
        height: 160
        x: (window.width - width) / 2
        y: (window.height - height) / 2
        background: Rectangle { radius: 8; color: "#ffffff"; border.color: "#d1d5db" }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 12
            Button {
                Layout.fillWidth: true
                text: "\u4ece\u6587\u4ef6\u5e93\u9009\u62e9"
                onClicked: {
                    fileSourcePopup.close()
                    libraryPickerPopup.open()
                }
            }
            Button {
                Layout.fillWidth: true
                text: "\u4ece\u672c\u5730\u9009\u62e9"
                onClicked: {
                    fileSourcePopup.close()
                    nativeFileDialog.open()
                }
            }
        }
    }

    Popup {
        id: libraryPickerPopup
        modal: true
        focus: true
        width: Math.min(window.width - 80, 720)
        height: Math.min(window.height - 80, 520)
        x: (window.width - width) / 2
        y: (window.height - height) / 2
        background: Rectangle { radius: 8; color: "#f5f6f7"; border.color: "#d1d5db" }

        FileDetailPage {
            anchors.fill: parent
            currentLibraryPath: window.currentLibraryPath
            pickerMode: true
            onFileChosen: function(path) {
                libraryPickerPopup.close()
                window.sendSelectedFile(path)
            }
        }
    }

    IncomingOfferPopup { id: offerPopup }

    Popup {
        id: profilePopup
        modal: true
        focus: true
        width: Math.min(window.width - 32, 360)
        height: 260
        x: 16
        y: 68
        background: Rectangle {
            radius: 18
            color: "#ffffff"
            border.color: "#e5e7eb"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 14

            RowLayout {
                spacing: 14
                Rectangle {
                    Layout.preferredWidth: 58
                    Layout.preferredHeight: 58
                    radius: 29
                    color: "#dbeafe"
                    Text {
                        anchors.centerIn: parent
                        text: (networkManager.myName || "?").slice(0, 1).toUpperCase()
                        font.pixelSize: 24
                        font.bold: true
                        color: "#1d4ed8"
                    }
                }
                Button {
                    Layout.preferredWidth: 82
                    Layout.preferredHeight: 30
                    text: "\u6e05\u7a7a\u8bb0\u5f55"
                    font.pixelSize: 12
                    onClicked: {
                        networkManager.clearLocalData()
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Text {
                        text: networkManager.myName || "\u672a\u547d\u540d"
                        color: "#111827"
                        font.pixelSize: 20
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                    Text {
                        text: networkManager.ecsConnected ? "\u516c\u7f51\u5728\u7ebf" : "\u5c40\u57df\u7f51\u6a21\u5f0f"
                        color: "#6b7280"
                        font.pixelSize: 13
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#eef0f3" }

            Text { text: "\u6587\u4ef6\u5e93"; color: "#6b7280"; font.pixelSize: 12 }
            Text {
                text: "\u9ed8\u8ba4\u6587\u4ef6\u5e93"
                color: "#111827"
                font.pixelSize: 15
                font.bold: true
            }

            Item { Layout.fillHeight: true }

            Button {
                Layout.fillWidth: true
                text: "\u9000\u51fa\u767b\u5f55"
                onClicked: {
                    profilePopup.close()
                    networkManager.logout()
                }
            }
        }
    }

    Popup {
        id: desktopMenuPopup
        modal: true
        focus: true
        width: 150
        height: 112
        x: 12
        y: Math.max(12, window.height - height - 16)
        background: Rectangle {
            radius: 8
            color: "#ffffff"
            border.color: "#d1d5db"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 8
            spacing: 6

            Button {
                Layout.fillWidth: true
                text: "\u6e05\u7a7a\u8bb0\u5f55"
                onClicked: {
                    desktopMenuPopup.close()
                    networkManager.clearLocalData()
                }
            }

            Button {
                Layout.fillWidth: true
                text: "\u9000\u51fa\u767b\u5f55"
                onClicked: {
                    desktopMenuPopup.close()
                    networkManager.logout()
                }
            }
        }
    }

    Rectangle {
        id: notifBar
        visible: false
        z: 100
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 10
        width: Math.min(parent.width * 0.72, 560)
        height: 42
        radius: 12
        color: "#202329"
        opacity: 0.95

        Text {
            id: notifText
            anchors.centerIn: parent
            width: parent.width - 28
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            color: "#f3f4f6"
            font.pixelSize: 13
        }

        Timer { id: notifTimer; interval: 3600; onTriggered: notifBar.visible = false }
        MouseArea { anchors.fill: parent; onClicked: notifBar.visible = false }
    }

    Component { id: conversationListComp; ConversationList { conversations: window.conversationData; currentIndex: window.currentConversationIndex; onConversationClicked: function(index) { window.selectConversationByIndex(index) } } }
    Component { id: contactListComp; ContactList { onPeerSelected: function(peerName, isLan) { window.selectedPeerName = peerName; window.selectedPeerIsLan = isLan } } }
    Component { id: transferListComp; TransferList { searchText: window.mobileMode ? window.mobileSearchText : "" } }
    Component { id: fileListComp; FileList { currentPath: window.currentLibraryPath; onLibrarySelected: function(path) { window.currentLibraryPath = path } } }
    Component { id: chatPageComp; ChatPage { conversationInfo: window.conversationData.length > 0 ? window.conversationData[window.currentConversationIndex] : null; showBackButton: window.mobileMode; canSendText: window.canSendTextTo(window.currentConversationName()); canSendFile: window.currentConversationName().length > 0 && window.currentConversationName() !== "LCFT"; onBackRequested: mobileChatStack.pop(); onOpenFilePicker: window.mobileMode ? nativeFileDialog.open() : fileSourcePopup.open(); onSendMessage: function(text) { window.sendMessageToCurrentConversation(text) } } }
    Component { id: contactDetailPageComp; ContactDetailPage { peerName: window.selectedPeerName; peerIsLan: window.selectedPeerIsLan; onOpenChat: function(peerName) { window.openConversation(peerName) } } }
    Component { id: transferDetailPageComp; TransferDetailPage {} }
    Component { id: fileDetailPageComp; FileDetailPage { currentLibraryPath: window.currentLibraryPath; searchText: window.mobileMode ? window.mobileSearchText : "" } }
    Component {
        id: mobileChatHomeComp
        MobileChatHome {
            conversations: window.conversationData
            currentIndex: window.currentConversationIndex
            searchText: window.mobileSearchText
            onConversationClicked: function(index) { window.selectConversationByIndex(index) }
            onOpenConversation: mobileChatStack.push(chatPageComp)
        }
    }
    Component {
        id: mobileDeviceComp
        MobileDevicePage {
            selectedPeerName: window.selectedPeerName
            selectedPeerIsLan: window.selectedPeerIsLan
            searchText: window.mobileSearchText
            onPeerSelected: function(peerName, isLan) {
                window.selectedPeerName = peerName
                window.selectedPeerIsLan = isLan
            }
            onOpenChat: function(peerName) { window.openConversation(peerName) }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0
        visible: loggedIn && !mobileMode

        Rectangle {
            Layout.preferredWidth: 61
            Layout.fillHeight: true
            color: "#202329"
            LeftNavBar {
                anchors.fill: parent
                currentTab: window.currentTab
                onTabClicked: function(index) { window.currentTab = index }
                onMenuRequested: desktopMenuPopup.open()
            }
        }

        Loader {
            id: middlePanelLoader
            Layout.preferredWidth: currentTab === 3 ? 0 : Math.max(220, (window.width - 61) / 3)
            Layout.fillHeight: true
            visible: currentTab !== 3
            sourceComponent: currentTab === 0 ? conversationListComp
                             : currentTab === 1 ? contactListComp
                             : currentTab === 2 ? transferListComp
                             : fileListComp
        }

        Loader {
            id: rightPanelLoader
            Layout.fillWidth: true
            Layout.preferredWidth: currentTab === 3 ? window.width - 61 : Math.max(440, (window.width - 61) * 2 / 3)
            Layout.fillHeight: true
            sourceComponent: currentTab === 0 ? chatPageComp
                             : currentTab === 1 ? contactDetailPageComp
                             : currentTab === 2 ? transferDetailPageComp
                             : fileDetailPageComp
        }
    }

    Rectangle {
        anchors.fill: parent
        visible: loggedIn && mobileMode
        color: "#eef3ff"

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                id: mobileHeader
                Layout.fillWidth: true
                Layout.preferredHeight: visible ? 148 : 0
                visible: !(window.mobileTab === 0 && mobileChatStack.depth > 1)
                color: "#eef3ff"

                RowLayout {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.leftMargin: 20
                    anchors.rightMargin: 18
                    anchors.topMargin: 18
                    spacing: 12

                    Rectangle {
                        Layout.preferredWidth: 54
                        Layout.preferredHeight: 54
                        radius: 27
                        color: "#ffffff"
                        border.color: "#dbe3f1"
                        MouseArea { anchors.fill: parent; onClicked: profilePopup.open() }
                        Text {
                            anchors.centerIn: parent
                            text: (networkManager.myName || "?").slice(0, 1).toUpperCase()
                            color: "#1d4ed8"
                            font.pixelSize: 22
                            font.bold: true
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text {
                            text: networkManager.myName || "\u6211\u7684\u8bbe\u5907"
                            color: "#111827"
                            font.pixelSize: 22
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: networkManager.ecsConnected ? "\u516c\u7f51\u5728\u7ebf" : "\u5c40\u57df\u7f51\u76f4\u8fde"
                            color: "#6b7280"
                            font.pixelSize: 13
                        }
                    }

                    ToolButton {
                        text: "+"
                        font.pixelSize: 34
                        onClicked: {
                            if (window.mobileTab === 1)
                                networkManager.discover()
                            else
                                nativeFileDialog.open()
                        }
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    anchors.bottomMargin: 16
                    height: 44
                    radius: 12
                    color: "#ffffff"
                    TextField {
                        id: mobileSearch
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: clearSearchButton.visible ? 48 : 16
                        text: window.mobileSearchText
                        placeholderText: "\u641c\u7d22"
                        font.pixelSize: 17
                        horizontalAlignment: TextInput.AlignHCenter
                        background: null
                        onTextChanged: window.mobileSearchText = text
                    }
                    ToolButton {
                        id: clearSearchButton
                        visible: window.mobileSearchText.length > 0
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: 6
                        width: 36
                        height: 36
                        text: "×"
                        onClicked: window.mobileSearchText = ""
                    }
                }
            }

            StackView {
                id: mobileChatStack
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: window.mobileTab === 0
                initialItem: mobileChatHomeComp
                pushEnter: Transition {}
                pushExit: Transition {}
                popEnter: Transition {}
                popExit: Transition {}
                replaceEnter: Transition {}
                replaceExit: Transition {}
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: window.mobileTab === 1
                active: visible
                sourceComponent: mobileDeviceComp
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: window.mobileTab === 2
                active: visible
                sourceComponent: transferListComp
            }

            Loader {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: window.mobileTab === 3
                active: visible
                sourceComponent: fileDetailPageComp
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 76
                color: "#ffffff"
                border.color: "#e5e7eb"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    spacing: 0

                    Repeater {
                        model: [
                            { label: "\u804a\u5929", mark: "\ud83d\udcac" },
                            { label: "\u8bbe\u5907\u53d1\u73b0", mark: "\ud83d\udce1" },
                            { label: "\u4f20\u8f93", mark: "\u2195" },
                            { label: "\u6587\u4ef6\u5e93", mark: "\ud83d\udcc1" }
                        ]
                        delegate: ToolButton {
                            id: mobileTabButton
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            text: modelData.mark + "\n" + modelData.label
                            font.pixelSize: 13
                            highlighted: window.mobileTab === index
                            background: Rectangle {
                                color: mobileTabButton.highlighted ? "#e5e7eb" : "transparent"
                            }
                            contentItem: Text {
                                text: mobileTabButton.text
                                color: "#111827"
                                font: mobileTabButton.font
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            onClicked: {
                                if (window.mobileTab !== index) {
                                    window.mobileTab = index
                                    if (index === 0) {
                                        mobileChatStack.clear(StackView.Immediate)
                                        mobileChatStack.push(mobileChatHomeComp, {}, StackView.Immediate)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}


import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"

    signal peerSelected(string peerName, bool isLan)
    signal openChat(string peerName)

    property string selectedPeerName: ""
    property bool selectedPeerIsLan: true
    property string searchText: ""

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: "#ffffff"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                Button { text: "\u53d1\u73b0"; onClicked: networkManager.discover() }
                Button { text: "\u5237\u65b0"; enabled: networkManager.ecsConnected; onClicked: networkManager.ecsListUsers() }
                Item { Layout.fillWidth: true }
            }
        }

        ListView {
            id: devices
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: {
                var rows = []
                var text = root.searchText.trim().toLowerCase()
                if ((networkManager.myName || "").length > 0) {
                    rows.push({
                        name: networkManager.myName,
                        displayName: networkManager.myName + "\uff08\u5f53\u524d\u8bbe\u5907\uff09",
                        detail: networkManager.connectionMode === 2 ? "\u5c40\u57df\u7f51\u6a21\u5f0f" : "\u516c\u7f51\u5728\u7ebf",
                        isLan: networkManager.connectionMode === 2,
                        isSelf: true
                    })
                }
                var lan = networkManager.discoveredPeers
                for (var i = 0; i < lan.length; ++i) {
                    if (lan[i].isSelf)
                        continue
                    rows.push({
                        name: lan[i].name,
                        displayName: lan[i].displayName || lan[i].name || "\u5c40\u57df\u7f51\u8bbe\u5907",
                        detail: "\u5c40\u57df\u7f51\u8bbe\u5907",
                        isLan: true,
                        isSelf: false
                    })
                }
                var ecs = networkManager.onlineUsers
                for (var j = 0; j < ecs.length; ++j) {
                    if (ecs[j].isSelf)
                        continue
                    rows.push({ name: ecs[j].name, displayName: ecs[j].displayName || ecs[j].name, detail: ecs[j].isSelf ? "\u5f53\u524d\u8bbe\u5907" : "\u516c\u7f51\u7528\u6237", isLan: false, isSelf: !!ecs[j].isSelf })
                }
                if (text.length === 0)
                    return rows
                return rows.filter(function(row) {
                    return (row.displayName || row.name || "").toLowerCase().indexOf(text) >= 0 ||
                           (row.detail || "").toLowerCase().indexOf(text) >= 0
                })
            }
            delegate: ItemDelegate {
                id: deviceDelegate
                width: devices.width
                height: 72
                highlighted: modelData.name === root.selectedPeerName && modelData.isLan === root.selectedPeerIsLan
                background: Rectangle {
                    color: deviceDelegate.highlighted ? "#eef2f7" : "transparent"
                }
                onClicked: {
                    root.selectedPeerName = modelData.name
                    root.selectedPeerIsLan = modelData.isLan
                    root.peerSelected(modelData.name, modelData.isLan)
                }
                contentItem: RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 46
                        Layout.preferredHeight: 46
                        radius: 23
                        color: modelData.isLan ? "#dbeafe" : "#dcfce7"
                        Text {
                            anchors.centerIn: parent
                            text: modelData.isLan ? "\ud83d\udce1" : "\ud83c\udf10"
                            color: modelData.isLan ? "#2563eb" : "#16a34a"
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Text { text: modelData.displayName || modelData.name; color: "#111827"; font.pixelSize: 17; font.bold: true; elide: Text.ElideRight; Layout.fillWidth: true }
                        Text { text: modelData.detail; color: "#8a9099"; font.pixelSize: 13; elide: Text.ElideRight; Layout.fillWidth: true }
                    }
                    Button {
                        id: sendButton
                        Layout.preferredWidth: 76
                        text: "\u53d1\u9001"
                        background: Rectangle {
                            radius: 6
                            color: (modelData.name === root.selectedPeerName && modelData.isLan === root.selectedPeerIsLan) ? "#e5e7eb" : "#f3f4f6"
                            border.color: "#d1d5db"
                        }
                        contentItem: Text {
                            text: sendButton.text
                            color: "#111827"
                            font: sendButton.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        onClicked: {
                            root.selectedPeerName = modelData.name
                            root.selectedPeerIsLan = modelData.isLan
                            root.peerSelected(modelData.name, modelData.isLan)
                            root.openChat(modelData.name)
                        }
                    }
                }
            }
        }
    }
}

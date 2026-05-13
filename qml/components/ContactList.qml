import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"
    signal peerSelected(string peerName, bool isLan)

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
                Text { text: "\u8bbe\u5907\u5217\u8868"; font.pixelSize: 20; font.bold: true; color: "#111827" }
                Item { Layout.fillWidth: true }
                SmallBlueButton { text: "\u53d1\u73b0"; onClicked: networkManager.discover() }
                SmallBlueButton { text: "\u5237\u65b0"; enabled: networkManager.ecsConnected; onClicked: networkManager.ecsListUsers() }
            }
        }

        ItemDelegate {
            Layout.fillWidth: true
            visible: networkManager.myName.length > 0
            text: networkManager.myName + "  (\u5f53\u524d\u8bbe\u5907)  " + (networkManager.connectionMode === 2 ? "\u5c40\u57df\u7f51\u6a21\u5f0f" : "\u516c\u7f51\u5728\u7ebf")
            onClicked: root.peerSelected(networkManager.myName, networkManager.connectionMode === 2)
        }

        Text {
            Layout.margins: 12
            text: "\u5c40\u57df\u7f51\u8bbe\u5907"
            color: "#6b7280"
            font.bold: true
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(220, contentHeight)
            clip: true
            model: networkManager.discoveredPeers
            delegate: ItemDelegate {
                width: ListView.view.width
                text: modelData.displayName || modelData.name || "\u5c40\u57df\u7f51\u8bbe\u5907"
                onClicked: root.peerSelected(modelData.name, true)
            }
        }

        Text {
            Layout.margins: 12
            text: "\u516c\u7f51\u7528\u6237"
            color: "#6b7280"
            font.bold: true
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: networkManager.onlineUsers
            delegate: ItemDelegate {
                width: ListView.view.width
                visible: !(typeof modelData !== "string" && modelData.isSelf)
                height: visible ? implicitHeight : 0
                text: typeof modelData === "string" ? modelData : (modelData.displayName || modelData.name)
                onClicked: root.peerSelected(typeof modelData === "string" ? text.replace("(\u672c\u673a)", "").replace("(present)", "") : modelData.name, false)
            }
        }
    }
}

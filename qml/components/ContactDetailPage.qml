import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#f5f6f7"
    property string peerName: ""
    property bool peerIsLan: true
    signal openChat(string peerName)

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
                text: root.peerName.length > 0 ? root.peerName : "\u8bbe\u5907\u8be6\u60c5"
                font.pixelSize: 20
                font.bold: true
                color: "#111827"
            }
        }

        ColumnLayout {
            anchors.margins: 28
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 16

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 130
                radius: 18
                color: "white"
                border.color: "#e5e7eb"

                Column {
                    anchors.centerIn: parent
                    spacing: 8
                    Text { text: root.peerName.length > 0 ? root.peerName : "\u672a\u9009\u62e9\u8bbe\u5907"; font.pixelSize: 24; font.bold: true; color: "#111827"; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: root.peerIsLan ? "\u5c40\u57df\u7f51\u76f4\u8fde\u4f20\u8f93" : "\u516c\u7f51\u4e2d\u8f6c\u4f20\u8f93"; font.pixelSize: 14; color: "#6b7280"; anchors.horizontalCenter: parent.horizontalCenter }
                }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                enabled: root.peerName.length > 0
                text: "\u53d1\u9001"
                highlighted: true
                onClicked: root.openChat(root.peerName)
            }

            Text {
                Layout.fillWidth: true
                text: root.peerIsLan
                      ? "\u5c40\u57df\u7f51\u6a21\u5f0f\u4f7f\u7528\u76f4\u8fde TCP \u5206\u5757\u534f\u8bae\uff0c\u624b\u673a\u548c\u7535\u8111\u53ef\u4ee5\u76f8\u4e92\u4f20\u6587\u4ef6\u3002"
                      : "\u516c\u7f51\u6a21\u5f0f\u4f7f\u7528\u73b0\u6709\u63a7\u5236\u670d\u52a1\u5668\u548c\u6570\u636e\u4e2d\u8f6c\u534f\u8bae\u3002"
                wrapMode: Text.WordWrap
                color: "#6b7280"
            }

            Item { Layout.fillHeight: true }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#202329"

    property int currentTab: 0
    signal tabClicked(int index)
    signal menuRequested()

    property real scaleFactor: width / 76

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 10 * root.scaleFactor
        anchors.bottomMargin: 10 * root.scaleFactor
        spacing: 14 * root.scaleFactor

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(root.width, 46 * root.scaleFactor)
            Layout.preferredHeight: Math.min(root.width, 46 * root.scaleFactor)
            radius: width / 2
            color: networkManager.ecsConnected ? "#2ea84f" : "#4c535f"

            Text {
                anchors.centerIn: parent
                text: networkManager.myName.length > 0 ? networkManager.myName.charAt(0) : "\u6211"
                color: "white"
                font.pixelSize: 16 * root.scaleFactor
                font.bold: true
            }
        }

        Text {
            Layout.alignment: Qt.AlignHCenter
            text: {
                if (networkManager.connectionMode === 1) return "\u516c\u7f51"
                if (networkManager.connectionMode === 2) return "\u5c40\u57df\u7f51"
                return "\u79bb\u7ebf"
            }
            color: networkManager.connectionMode > 0 ? "#07c160" : "#666"
            font.pixelSize: Math.max(6, 9 * root.scaleFactor)
            font.bold: true
        }

        Item { Layout.preferredHeight: 6 * root.scaleFactor }

        Repeater {
            model: [
                { label: "\u804a\u5929", icon: "\ud83d\udcac" },
                { label: "\u8bbe\u5907", icon: "\ud83d\udce1" },
                { label: "\u4f20\u8f93", icon: "\u21c5" },
                { label: "\u6587\u4ef6", icon: "\ud83d\udcc1" }
            ]

            delegate: Item {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: root.width
                Layout.preferredHeight: 52 * root.scaleFactor

                Column {
                    anchors.centerIn: parent
                    spacing: 2 * root.scaleFactor

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.icon
                        color: root.currentTab === index ? "#07c160" : "#aab2bd"
                        font.pixelSize: 18 * root.scaleFactor
                        font.bold: true
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: modelData.label
                        color: root.currentTab === index ? "#07c160" : "#aab2bd"
                        font.pixelSize: Math.max(6, 10 * root.scaleFactor)
                    }

                    Rectangle {
                        visible: index === 2 && networkManager.transfers.length > 0
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 16
                        height: 10
                        radius: 5
                        color: "#ef4444"
                        Text {
                            anchors.centerIn: parent
                            text: networkManager.transfers.length
                            font.pixelSize: 8
                            color: "white"
                            font.bold: true
                        }
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.tabClicked(index)
                }
            }
        }

        Item { Layout.fillHeight: true }

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.max(32, 42 * root.scaleFactor)
            Layout.preferredHeight: Math.max(28, 34 * root.scaleFactor)
            radius: 6 * root.scaleFactor
            color: "transparent"
            border.color: "#68707d"
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: "\u00b7\u00b7\u00b7"
                color: "#aab2bd"
                font.pixelSize: Math.max(14, 18 * root.scaleFactor)
                font.bold: true
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.menuRequested()
            }
        }
    }
}

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#ffffff"
    signal librarySelected(string path)
    property string currentPath: networkManager.receivedFilesDir
    property var libraries: libraryManager.libraries && libraryManager.libraries.length > 0
                            ? libraryManager.libraries
                            : [networkManager.receivedFilesDir]

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
                Text { text: "\u6587\u4ef6\u5e93"; font.pixelSize: 20; font.bold: true; color: "#111827" }
                Item { Layout.fillWidth: true }
                Text { text: root.libraries.length + " \u4e2a"; color: "#8a9099"; font.pixelSize: 12 }
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.libraries
            delegate: ItemDelegate {
                width: ListView.view.width
                height: 64
                highlighted: modelData === root.currentPath
                background: Rectangle {
                    color: highlighted ? "#eef2f7" : "transparent"
                }
                contentItem: ColumnLayout {
                    spacing: 3
                    Text {
                        Layout.fillWidth: true
                        text: index === 0 ? "\u9ed8\u8ba4\u6587\u4ef6\u5e93" : modelData.split(/[\\/]/).pop()
                        color: "#111827"
                        font.pixelSize: 15
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: "#8a9099"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }
                onClicked: root.librarySelected(modelData)
            }
        }
    }
}

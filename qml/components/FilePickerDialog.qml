import QtQuick
import QtQuick.Controls

Popup {
    id: root
    signal chooseLocalFile()
    width: 320
    height: 160
    modal: true
    focus: true
    Column {
        anchors.centerIn: parent
        spacing: 14
        Text { text: "\u9009\u62e9\u8981\u53d1\u9001\u7684\u672c\u5730\u6587\u4ef6" }
        Button { text: "\u6253\u5f00\u6587\u4ef6\u9009\u62e9"; onClicked: { root.close(); root.chooseLocalFile() } }
    }
}

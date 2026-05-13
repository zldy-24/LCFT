import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#1a1b1e"

    signal loginSuccess()

    property bool showEcsForm: false
    property bool showLanForm: false
    property bool isConnecting: false
    property string errorMessage: ""

    function doEcsLogin() {
        if (ecsUsername.text.trim().length === 0 || ecsPassword.text.trim().length === 0 || root.isConnecting)
            return
        root.isConnecting = true
        root.errorMessage = ""
        networkManager.loginEcs(ecsUsername.text.trim(), ecsPassword.text.trim())
    }

    function doLanLogin() {
        if (lanDisplayName.text.trim().length === 0)
            return
        networkManager.startLanMode(lanDisplayName.text.trim())
    }

    Connections {
        target: networkManager
        function onLoginResult(success, error) {
            root.isConnecting = false
            if (success) {
                root.loginSuccess()
            } else {
                root.errorMessage = error || "\u767b\u5f55\u5931\u8d25"
            }
        }
        function onConnectionModeChanged() {
            if (networkManager.connectionMode === 2)
                root.loginSuccess()
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1a1b1e" }
            GradientStop { position: 0.5; color: "#1e2a1e" }
            GradientStop { position: 1.0; color: "#1a1b1e" }
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.85, 390)
        spacing: 14

        Text {
            text: "\u5c40\u57df\u7f51 / \u516c\u7f51\u4f20\u8f93"
            font.pixelSize: 32
            font.bold: true
            color: "#e8e8e8"
            Layout.alignment: Qt.AlignHCenter
        }

        Text {
            text: "\u5c40\u57df\u7f51 / \u516c\u7f51 \u6587\u4ef6\u4f20\u8f93\u5668"
            font.pixelSize: 14
            color: "#8b949e"
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 20
        }

        ColumnLayout {
            visible: !root.showEcsForm && !root.showLanForm
            Layout.fillWidth: true
            spacing: 14

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                text: "\ud83c\udf10  \u516c\u7f51\u6a21\u5f0f"
                onClicked: { root.showEcsForm = true; root.errorMessage = "" }
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                text: "\ud83d\udce1  \u5c40\u57df\u7f51\u6a21\u5f0f"
                onClicked: { root.showLanForm = true; root.errorMessage = "" }
            }

            Text {
                Layout.fillWidth: true
                text: "\u516c\u7f51\u6a21\u5f0f\u901a\u8fc7\u670d\u52a1\u5668\u4e2d\u8f6c\uff0c\u5c40\u57df\u7f51\u6a21\u5f0f\u5728\u540c\u4e00\u7f51\u7edc\u5185\u76f4\u8fde\u4f20\u8f93"
                font.pixelSize: 12
                color: "#6b7280"
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }

        ColumnLayout {
            visible: root.showEcsForm
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "\u516c\u7f51\u6a21\u5f0f\u767b\u5f55"
                font.pixelSize: 20
                font.bold: true
                color: "#e0e0e0"
                Layout.alignment: Qt.AlignHCenter
            }

            TextField {
                id: ecsUsername
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                placeholderText: "\u7528\u6237\u540d"
                Keys.onReturnPressed: root.doEcsLogin()
            }

            TextField {
                id: ecsPassword
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                placeholderText: "\u5bc6\u7801"
                echoMode: TextInput.Password
                Keys.onReturnPressed: root.doEcsLogin()
            }

            Text {
                visible: root.errorMessage.length > 0
                text: "\u26a0 " + root.errorMessage
                color: "#ef4444"
                Layout.alignment: Qt.AlignHCenter
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                enabled: ecsUsername.text.trim().length > 0 && ecsPassword.text.trim().length > 0 && !root.isConnecting
                text: root.isConnecting ? "\u8fde\u63a5\u4e2d..." : "\u767b  \u5f55"
                onClicked: root.doEcsLogin()
            }

            Button {
                Layout.preferredWidth: 78
                Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignLeft
                text: "\u2190 \u8fd4\u56de"
                flat: true
                contentItem: Text {
                    text: parent.text
                    color: "#ffffff"
                    font: parent.font
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: { root.showEcsForm = false; root.errorMessage = "" }
            }
        }

        ColumnLayout {
            visible: root.showLanForm
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "\u5c40\u57df\u7f51\u6a21\u5f0f"
                font.pixelSize: 20
                font.bold: true
                color: "#e0e0e0"
                Layout.alignment: Qt.AlignHCenter
            }

            TextField {
                id: lanDisplayName
                Layout.fillWidth: true
                Layout.preferredHeight: 46
                placeholderText: "\u4e3a\u5f53\u524d\u8bbe\u5907\u547d\u540d"
                Keys.onReturnPressed: root.doLanLogin()
            }

            Button {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                enabled: lanDisplayName.text.trim().length > 0
                text: "\u8fdb  \u5165"
                onClicked: root.doLanLogin()
            }

            Button {
                Layout.preferredWidth: 78
                Layout.preferredHeight: 28
                Layout.alignment: Qt.AlignLeft
                text: "\u2190 \u8fd4\u56de"
                flat: true
                contentItem: Text {
                    text: parent.text
                    color: "#ffffff"
                    font: parent.font
                    horizontalAlignment: Text.AlignLeft
                    verticalAlignment: Text.AlignVCenter
                }
                onClicked: { root.showLanForm = false; root.errorMessage = "" }
            }
        }
    }

    Button {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 18
        anchors.bottomMargin: 18
        width: 150
        height: 34
        visible: !root.showEcsForm && !root.showLanForm
        text: "\u6e05\u7a7a\u6240\u6709\u672c\u5730\u8bb0\u5f55"
        font.pixelSize: 12
        onClicked: networkManager.clearAllLocalData()
    }
}

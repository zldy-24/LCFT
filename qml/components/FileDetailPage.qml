import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.folderlistmodel

Rectangle {
    id: root
    color: "#f5f6f7"
    property string currentLibraryPath: ""
    property string currentFolderPath: currentLibraryPath
    property string searchText: ""
    property bool pickerMode: false
    property bool folderPickerMode: false
    property bool allowReceivePathPicker: true
    signal fileChosen(string path)
    signal folderChosen(string path)
    property string contextFilePath: ""
    property string contextFileName: ""
    property bool contextFileIsDir: false
    property string currentFolderUrl: {
        var normalized = normalizePath(currentFolderPath)
        if (!normalized)
            return ""
        return normalized.charAt(0) === "/" ? "file://" + normalized : "file:///" + normalized
    }

    function normalizePath(path) {
        if (!path)
            return ""
        var value = decodeURIComponent(("" + path).replace(/\\/g, "/"))
        if (value.indexOf("file:///") === 0)
            value = value.slice(8)
        else if (value.indexOf("file://") === 0)
            value = value.slice(7)
        while (value.length > 1 && value.endsWith("/"))
            value = value.slice(0, -1)
        return value
    }

    function pathToUrl(path) {
        var normalized = normalizePath(path)
        if (!normalized)
            return ""
        return normalized.charAt(0) === "/" ? "file://" + normalized : "file:///" + normalized
    }

    function setFolder(path) {
        var rootPath = normalizePath(root.currentLibraryPath)
        var nextPath = normalizePath(path)
        if (nextPath === rootPath || nextPath.indexOf(rootPath + "/") === 0)
            root.currentFolderPath = nextPath
        else
            root.currentFolderPath = rootPath
    }

    function reloadCurrentFolder() {
        var path = root.currentFolderPath
        root.currentFolderPath = ""
        Qt.callLater(function() { root.currentFolderPath = path })
    }

    function submitNewFolder() {
        if (newFolderName.text.trim().length === 0)
            return
        if (networkManager.createLibraryFolder(root.currentFolderPath, newFolderName.text.trim())) {
            newFolderName.clear()
            newFolderPopup.close()
            root.reloadCurrentFolder()
        }
    }

    Popup {
        id: newFolderPopup
        modal: true
        focus: true
        width: Math.min(root.width - 40, 320)
        height: 150
        x: (root.width - width) / 2
        y: 86
        background: Rectangle {
            radius: 8
            color: "#ffffff"
            border.color: "#d1d5db"
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 10

            Text {
                text: "\u65b0\u5efa\u6587\u4ef6\u5939"
                color: "#111827"
                font.pixelSize: 16
                font.bold: true
            }

            TextField {
                id: newFolderName
                Layout.fillWidth: true
                placeholderText: "\u6587\u4ef6\u5939\u540d"
                selectByMouse: true
                Keys.onReturnPressed: root.submitNewFolder()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Item { Layout.fillWidth: true }
                Button {
                    text: "\u53d6\u6d88"
                    onClicked: newFolderPopup.close()
                }
                Button {
                    id: createFolderButton
                    text: "\u65b0\u5efa"
                    enabled: newFolderName.text.trim().length > 0
                    onClicked: root.submitNewFolder()
                }
            }
        }
    }

    Popup {
        id: receivePathPopup
        modal: true
        focus: true
        width: Math.min(root.width - 60, 720)
        height: Math.min(root.height - 60, 520)
        x: (root.width - width) / 2
        y: (root.height - height) / 2
        background: Rectangle { radius: 8; color: "#f5f6f7"; border.color: "#d1d5db" }

        Loader {
            anchors.fill: parent
            active: receivePathPopup.visible
            sourceComponent: Component {
                FolderSelectPage {
                    currentLibraryPath: root.currentLibraryPath
                    onFolderChosen: function(path) {
                        networkManager.setReceiveTargetDir(path)
                        receivePathPopup.close()
                    }
                }
            }
        }
    }

    Popup {
        id: sendToPopup
        modal: true
        focus: true
        width: Math.min(root.width - 48, 320)
        height: Math.min(root.height - 80, 420)
        x: (root.width - width) / 2
        y: 84
        background: Rectangle { radius: 8; color: "#ffffff"; border.color: "#d1d5db" }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 8
            Text {
                text: "\u53d1\u9001\u7ed9"
                color: "#111827"
                font.pixelSize: 16
                font.bold: true
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: root.sendTargets()
                delegate: ItemDelegate {
                    width: ListView.view.width
                    text: modelData.name + (modelData.isLan ? "  LAN" : "  ECS")
                    onClicked: {
                        if (modelData.isLan)
                            networkManager.lanSendFile(modelData.name, root.contextFilePath)
                        else
                            networkManager.ecsSendFile(modelData.name, root.contextFilePath)
                        sendToPopup.close()
                    }
                }
                Text {
                    anchors.centerIn: parent
                    visible: parent.count === 0
                    text: "\u6682\u65e0\u53ef\u53d1\u9001\u5bf9\u8c61"
                    color: "#9ca3af"
                }
            }
        }
    }

    Menu {
        id: fileContextMenu
        MenuItem {
            text: "\u590d\u5236"
            onTriggered: networkManager.copyLibraryItem(root.contextFilePath)
        }
        MenuItem {
            text: "\u53d1\u9001\u7ed9"
            enabled: !root.contextFileIsDir
            onTriggered: sendToPopup.open()
        }
        MenuItem {
            text: "\u5220\u9664"
            onTriggered: {
                if (networkManager.deleteLibraryItem(root.contextFilePath))
                    root.reloadCurrentFolder()
            }
        }
    }

    Menu {
        id: blankContextMenu
        MenuItem {
            text: "\u7c98\u8d34"
            onTriggered: {
                if (networkManager.pasteLibraryItem(root.currentFolderPath))
                    root.reloadCurrentFolder()
            }
        }
    }

    onCurrentLibraryPathChanged: currentFolderPath = currentLibraryPath

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
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 10

                ToolButton {
                    text: "<"
                    enabled: root.normalizePath(root.currentFolderPath) !== root.normalizePath(root.currentLibraryPath)
                    onClicked: {
                        var path = root.normalizePath(root.currentFolderPath)
                        var rootPath = root.normalizePath(root.currentLibraryPath)
                        var slash = path.lastIndexOf("/")
                        root.currentFolderPath = slash > rootPath.length ? path.slice(0, slash) : rootPath
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: "\u6587\u4ef6\u5e93"
                        color: "#111827"
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Text {
                        text: folderModel.count + " \u9879"
                        color: "#8a9099"
                        font.pixelSize: 12
                    }
                }

                Button {
                    visible: root.folderPickerMode
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 30
                    text: "\u9009\u62e9\u6b64\u76ee\u5f55"
                    font.pixelSize: 12
                    onClicked: root.folderChosen(root.currentFolderPath)
                }

                Button {
                    visible: !root.pickerMode && !root.folderPickerMode
                    Layout.preferredWidth: 96
                    Layout.preferredHeight: 30
                    text: "\u65b0\u5efa\u6587\u4ef6\u5939"
                    font.pixelSize: 12
                    onClicked: {
                        newFolderName.clear()
                        newFolderPopup.open()
                        newFolderName.forceActiveFocus()
                    }
                }

                Button {
                    visible: root.allowReceivePathPicker && !root.pickerMode && !root.folderPickerMode
                    Layout.preferredWidth: 110
                    Layout.preferredHeight: 30
                    text: "\u9ed8\u8ba4\u63a5\u6536\u8def\u5f84"
                    font.pixelSize: 12
                    onClicked: receivePathPopup.open()
                }
            }
        }

        ListView {
            id: fileList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: FolderListModel {
                id: folderModel
                folder: root.currentFolderUrl
                showDirs: true
                showFiles: true
                showDotAndDotDot: false
                sortField: FolderListModel.Type
            }
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.RightButton
                z: -1
                onClicked: blankContextMenu.popup()
            }
            delegate: ItemDelegate {
                width: fileList.width
                visible: model.fileName !== "part" &&
                         (root.searchText.trim().length === 0 ||
                          model.fileName.toLowerCase().indexOf(root.searchText.trim().toLowerCase()) >= 0)
                height: visible ? 58 : 0
                contentItem: RowLayout {
                    spacing: 12
                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 8
                        color: model.fileIsDir ? "#dbeafe" : "#dcfce7"
                        Text {
                            anchors.centerIn: parent
                            text: model.fileIsDir ? "\ud83d\udcc1" : "\ud83d\udcc4"
                            color: model.fileIsDir ? "#2563eb" : "#16a34a"
                            font.bold: true
                        }
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            text: model.fileName
                            color: "#111827"
                            font.pixelSize: 15
                            font.bold: true
                            elide: Text.ElideRight
                            Layout.fillWidth: true
                        }
                        Text {
                            text: model.fileIsDir ? "\u6587\u4ef6\u5939" : "\u6587\u4ef6"
                            color: "#8a9099"
                            font.pixelSize: 12
                        }
                    }
                }
                onClicked: {
                    if (model.fileIsDir)
                        root.setFolder(model.filePath)
                    else if (root.pickerMode)
                        root.fileChosen(model.filePath)
                    else
                        Qt.openUrlExternally(root.pathToUrl(model.filePath))
                }
                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.RightButton
                    onClicked: {
                        root.contextFilePath = model.filePath
                        root.contextFileName = model.fileName
                        root.contextFileIsDir = model.fileIsDir
                        fileContextMenu.popup()
                    }
                }
            }

            Text {
                anchors.centerIn: parent
                visible: fileList.count === 0
                text: "\u6682\u65e0\u6587\u4ef6"
                color: "#9ca3af"
                font.pixelSize: 15
            }
        }
    }

    function sendTargets() {
        var result = []
        var users = networkManager.onlineUsers
        for (var i = 0; i < users.length; ++i) {
            if (!users[i].isSelf && users[i].name)
                result.push({ name: users[i].name, isLan: false })
        }
        var peers = networkManager.discoveredPeers
        for (var p = 0; p < peers.length; ++p) {
            if (!peers[p].isSelf && peers[p].name)
                result.push({ name: peers[p].name, isLan: true })
        }
        return result
    }
}

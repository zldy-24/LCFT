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
    signal fileChosen(string path)
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
            delegate: ItemDelegate {
                width: fileList.width
                visible: root.searchText.trim().length === 0 || model.fileName.toLowerCase().indexOf(root.searchText.trim().toLowerCase()) >= 0
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
}

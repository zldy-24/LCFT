#ifndef FILESYSTEMMODELMANAGER_H
#define FILESYSTEMMODELMANAGER_H

#include <QObject>
#include <QFileSystemModel>

class FileSystemModelManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QFileSystemModel* model READ model CONSTANT)

public:
    explicit FileSystemModelManager(QObject *parent = nullptr);

    QFileSystemModel* model() const { return m_model; }

    // 为 QML 提供角色枚举
    enum Roles {
        FilePathRole = QFileSystemModel::FilePathRole,
        FileNameRole = QFileSystemModel::FileNameRole,
        IsDirRole = QFileSystemModel::FilePermissions  // 实际用不到，我们用 hasChildren
    };
    Q_ENUM(Roles)

private:
    QFileSystemModel* m_model;
};

#endif

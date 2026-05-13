#ifndef FILETREEMODEL_H
#define FILETREEMODEL_H

#include <QAbstractItemModel>
#include <QFileInfo>
#include <QList>
#include <QDir>

class FileTreeItem
{
public:
    FileTreeItem(const QFileInfo &info, FileTreeItem *parent = nullptr);
    ~FileTreeItem();

    FileTreeItem *parent() const { return m_parent; }
    QFileInfo info() const { return m_info; }
    bool isDir() const { return m_info.isDir(); }
    QString name() const { return m_info.fileName(); }
    QString path() const { return m_info.absoluteFilePath(); }

    void appendChild(FileTreeItem *child);
    FileTreeItem *child(int row) const;
    int childCount() const { return m_children.size(); }
    int row() const;
    bool childrenPopulated() const { return m_populated; }
    void setChildrenPopulated(bool populated) { m_populated = populated; }

private:
    QList<FileTreeItem*> m_children;
    FileTreeItem *m_parent;
    QFileInfo m_info;
    bool m_populated = false; // 是否已加载子节点
};

class FileTreeModel : public QAbstractItemModel
{
    Q_OBJECT
    Q_PROPERTY(QString rootPath READ rootPath WRITE setRootPath NOTIFY rootPathChanged)

public:
    explicit FileTreeModel(QObject *parent = nullptr);
    ~FileTreeModel();

    QString rootPath() const { return m_rootPath; }
    void setRootPath(const QString &path);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

signals:
    void rootPathChanged();

private:
    void populateChildren(FileTreeItem *item);
    FileTreeItem *getItem(const QModelIndex &index) const;

    QString m_rootPath;
    FileTreeItem *m_rootItem = nullptr;
};

#endif

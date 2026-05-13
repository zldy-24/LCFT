#include "filetreemodel.h"
#include <QDebug>
#include <algorithm>

FileTreeItem::FileTreeItem(const QFileInfo &info, FileTreeItem *parent)
    : m_parent(parent), m_info(info)
{
}

FileTreeItem::~FileTreeItem()
{
    qDeleteAll(m_children);
}

void FileTreeItem::appendChild(FileTreeItem *child)
{
    m_children.append(child);
}

FileTreeItem *FileTreeItem::child(int row) const
{
    return m_children.value(row);
}

int FileTreeItem::row() const
{
    if (m_parent)
        return m_parent->m_children.indexOf(const_cast<FileTreeItem*>(this));
    return 0;
}

FileTreeModel::FileTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

FileTreeModel::~FileTreeModel()
{
    delete m_rootItem;
}

void FileTreeModel::setRootPath(const QString &path)
{
    if (m_rootPath == path)
        return;

    beginResetModel();
    m_rootPath = path;
    delete m_rootItem;
    QFileInfo rootInfo(path);
    m_rootItem = new FileTreeItem(rootInfo);
    // 不立即加载子节点，等待 TreeView 展开时 fetchMore
    endResetModel();
    emit rootPathChanged();
}

FileTreeItem *FileTreeModel::getItem(const QModelIndex &index) const
{
    if (index.isValid())
        return static_cast<FileTreeItem*>(index.internalPointer());
    return m_rootItem;
}

QModelIndex FileTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    FileTreeItem *parentItem = getItem(parent);
    FileTreeItem *childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);
    return QModelIndex();
}

QModelIndex FileTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    FileTreeItem *childItem = getItem(child);
    FileTreeItem *parentItem = childItem->parent();
    if (parentItem == m_rootItem)
        return QModelIndex();
    return createIndex(parentItem->row(), 0, parentItem);
}

int FileTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;
    FileTreeItem *parentItem = getItem(parent);
    return parentItem->childCount();
}

int FileTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QVariant FileTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    FileTreeItem *item = getItem(index);
    switch (role) {
    case Qt::DisplayRole:
        return item->name();
    case Qt::UserRole + 1: // FilePathRole
        return item->path();
    case Qt::UserRole + 2: // IsDirRole
        return item->isDir();
    default:
        return QVariant();
    }
}

QHash<int, QByteArray> FileTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[Qt::DisplayRole] = "display";
    roles[Qt::UserRole + 1] = "filePath";
    roles[Qt::UserRole + 2] = "isDir";
    return roles;
}

bool FileTreeModel::canFetchMore(const QModelIndex &parent) const
{
    FileTreeItem *item = getItem(parent);
    // 如果父项是目录，且尚未加载子项，则可以 fetchMore
    return item && item->isDir() && !item->childrenPopulated();
}

void FileTreeModel::fetchMore(const QModelIndex &parent)
{
    FileTreeItem *item = getItem(parent);
    if (!item || !item->isDir() || item->childrenPopulated())
        return;

    // 加载子项
    QDir dir(item->path());
    QFileInfoList entries = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot,
                                              QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    if (entries.isEmpty()) {
        item->setChildrenPopulated(true);
        return;
    }

    beginInsertRows(parent, 0, entries.size() - 1);
    for (const QFileInfo &info : entries) {
        FileTreeItem *child = new FileTreeItem(info, item);
        item->appendChild(child);
    }
    item->setChildrenPopulated(true);
    endInsertRows();
}

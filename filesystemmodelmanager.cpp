#include "filesystemmodelmanager.h"
#include <QDir>

FileSystemModelManager::FileSystemModelManager(QObject *parent)
    : QObject(parent)
    , m_model(new QFileSystemModel(this))
{
    m_model->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);
    m_model->setRootPath(QDir::rootPath());
    m_model->setReadOnly(true);
    // 关闭文件监视，减少 UI 负担
    m_model->setOption(QFileSystemModel::DontWatchForChanges, true);
}

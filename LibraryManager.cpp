#include "LibraryManager.h"
#include <QDir>
#include <QDebug>

LibraryManager::LibraryManager(QObject *parent)
    : QObject(parent)
    , m_settings("LanChatShell", "FileLibraries")
{
    load();
}

QStringList LibraryManager::libraries() const
{
    return m_libraries;
}

void LibraryManager::addLibrary(const QString &path)
{
    QString cleanPath = QDir::cleanPath(path);
    if (!m_libraries.contains(cleanPath) && QDir(cleanPath).exists()) {
        m_libraries.append(cleanPath);
        save();
        emit librariesChanged();
    }
}

void LibraryManager::removeLibrary(int index)
{
    if (index >= 0 && index < m_libraries.size()) {
        m_libraries.removeAt(index);
        save();
        emit librariesChanged();
    }
}

QString LibraryManager::libraryPath(int index) const
{
    if (index >= 0 && index < m_libraries.size())
        return m_libraries.at(index);
    return QString();
}

void LibraryManager::load()
{
    m_libraries = m_settings.value("paths").toStringList();
    m_libraries.erase(std::remove_if(m_libraries.begin(), m_libraries.end(),
                                     [](const QString &path) { return !QDir(path).exists(); }),
                      m_libraries.end());
}

void LibraryManager::save()
{
    m_settings.setValue("paths", m_libraries);
}

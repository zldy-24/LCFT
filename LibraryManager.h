#ifndef LIBRARYMANAGER_H
#define LIBRARYMANAGER_H

#include <QObject>
#include <QStringList>
#include <QSettings>

class LibraryManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList libraries READ libraries NOTIFY librariesChanged)

public:
    explicit LibraryManager(QObject *parent = nullptr);

    QStringList libraries() const;

    Q_INVOKABLE void addLibrary(const QString &path);
    Q_INVOKABLE void removeLibrary(int index);
    Q_INVOKABLE QString libraryPath(int index) const;

signals:
    void librariesChanged();

private:
    void load();
    void save();

    QStringList m_libraries;
    QSettings m_settings;
};

#endif // LIBRARYMANAGER_H

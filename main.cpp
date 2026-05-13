#include <QDir>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QtQuickControls2/QQuickStyle>

#include "LibraryManager.h"
#include "NetworkManager.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(":/assets/app_icon.png")));
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    LibraryManager libraryManager;
    NetworkManager networkManager;
    QDir().mkpath(networkManager.receivedFilesDir());
    libraryManager.addLibrary(networkManager.receivedFilesDir());
    QObject::connect(&networkManager, &NetworkManager::receivedFilesDirChanged,
                     &libraryManager, [&]() {
                         QDir().mkpath(networkManager.receivedFilesDir());
                         libraryManager.addLibrary(networkManager.receivedFilesDir());
                     });

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("libraryManager"), &libraryManager);
    engine.rootContext()->setContextProperty(QStringLiteral("networkManager"), &networkManager);

    const QUrl url(QStringLiteral("qrc:/qt/qml/LanChatShell/qml/Main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);
    engine.load(url);
    return app.exec();
}

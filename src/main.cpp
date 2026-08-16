#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "XdrClient.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName("FredRadio");
    QCoreApplication::setApplicationName("XDR Tablet");
    QGuiApplication::setWindowIcon(QIcon(":/icons/XdrTablet.png"));

    XdrClient client;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("xdrClient", &client);
    engine.loadFromModule("XdrTablet", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

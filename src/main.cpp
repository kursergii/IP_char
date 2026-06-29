#include "controller.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QDebug>
#include <cstdio>

static void stdoutMessageHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    printf("%s\n", msg.toLocal8Bit().constData());
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(stdoutMessageHandler);
    QGuiApplication app(argc, argv);

    // Fusion style: consistent look across desktop and the Raspberry Pi touchscreen
    QQuickStyle::setStyle("Fusion");

    // -sim / --sim : run with synthesised data, no hardware required
    const QStringList args = QCoreApplication::arguments();
    const bool simulate = args.contains("-sim") || args.contains("--sim");

    Controller controller(simulate);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    engine.loadFromModule("WM", "Main");
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}

#include "wm.h"
#include <QApplication>
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
    QApplication a(argc, argv);
    WM w;
    w.show();

    return a.exec();
}

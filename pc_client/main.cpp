#include "pcclient.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    PcClient w;
    w.show();
    return app.exec();
}

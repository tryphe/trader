#include <QCoreApplication>

#include "tester.h"

int main(int argc, char *argv[])
{
    //qInstallMessageHandler( messageOutput );
    QCoreApplication a(argc, argv);

    Tester *t = new Tester(); Q_UNUSED( t )

    return a.exec();
}

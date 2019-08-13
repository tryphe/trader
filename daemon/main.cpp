#include "global.h"
#include "trader.h"

#include <QCoreApplication>
#include <QString>

int main( qint32 argc, char *argv[] )
{
    // set message handler
    qInstallMessageHandler( Global::messageOutput );

    // start qapp
    QCoreApplication a( argc, argv );

    // start trader
    Trader *trader = new Trader(); Q_UNUSED( trader )

    int ret = a.exec();

    kDebug() << QString( "main() done, code %1.").arg( ret );

    return ret;
}

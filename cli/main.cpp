#include "commandcaller.h"

#include <QCoreApplication>
#include <QString>
#include <QByteArray>

int main( int argc, char *argv[] )
{
    QCoreApplication a( argc, argv );

    // read args
    QStringList args = QCoreApplication::arguments();

    // strip binary name
    args.removeFirst();

    // form arguments for command caller
    QString joined = args.join( QChar( ' ' ) );

    CommandCaller c( joined.toLocal8Bit() );

    return 0;
}

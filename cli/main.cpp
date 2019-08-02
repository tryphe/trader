#include "commandcaller.h"

#include <QCoreApplication>
#include <QString>
#include <QByteArray>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // read args
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // remove application name

    // form arguments for command caller
    QString exchange = args.size() == 0 ? QLatin1String() : args.takeFirst();
    QString joined = args.join( QChar( ' ' ) );

    CommandCaller *c = new CommandCaller( exchange, joined.toLocal8Bit() );
    delete c;

    return 0;
}

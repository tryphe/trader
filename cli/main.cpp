#include "commandcaller.h"

#include <QCoreApplication>
#include <QString>
#include <QByteArray>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    // read args
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // remove application name

    // join args
    QString joined = args.join( QChar( ' ' ) );

    //qDebug() << joined;

    CommandCaller *c = new CommandCaller( joined.toLocal8Bit() );
    delete c;

    return 0;
}

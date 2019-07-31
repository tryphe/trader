#include "commandcaller.h"
#include "../daemon/global.h"

#include <QCoreApplication>

CommandCaller::CommandCaller( QByteArray command, QObject *parent )
    : QLocalSocket( parent )
{
    QLocalSocket::connectToServer( Global::getIPCPath() );
    QLocalSocket::waitForConnected();

    if ( state() == QLocalSocket::ConnectedState )
    {
        QLocalSocket::write( command );
        QLocalSocket::waitForBytesWritten();
    }
    else
    {
        qDebug() << "error: failed to send command:" << QLocalSocket::errorString();
    }
}

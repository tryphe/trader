#include "commandcaller.h"
#include "../daemon/global.h"

#include <QCoreApplication>

CommandCaller::CommandCaller( const QByteArray &command, QObject *parent )
    : QLocalSocket( parent )
{
    // open ipc
    const QString ipc_path = Global::getIPCPath();
    QLocalSocket::connectToServer( ipc_path );
    QLocalSocket::waitForConnected();

    // send command
    if ( state() == QLocalSocket::ConnectedState )
    {
        QLocalSocket::write( command );
        QLocalSocket::flush();
        QLocalSocket::close();
    }
    else // abort if we didn't connect
    {
        qDebug() << "error: failed to send command:" << QLocalSocket::errorString();
    }
}

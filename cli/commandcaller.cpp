#include "commandcaller.h"
#include "../daemon/global.h"

#include <QCoreApplication>

CommandCaller::CommandCaller( QString exchange, QByteArray command, QObject *parent )
    : QLocalSocket( parent )
{
    // parse the first argument as the daemon target
    QString subpath;
    if ( exchange.toLower() == "binance" ) subpath = BINANCE_SUBPATH;
    else if ( exchange.toLower() == "bittrex" ) subpath = BITTREX_SUBPATH;
    else if ( exchange.toLower() == "poloniex" ) subpath = POLONIEX_SUBPATH;
    else
    {
        qDebug() << "error: an incorrect exchange was given, use `trader-cli [bittrex|poloniex|binance] [command]`";
        return;
    }

    QString path = Global::getIPCPath( subpath );
    QLocalSocket::connectToServer( path );
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

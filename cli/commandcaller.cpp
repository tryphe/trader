#include "commandcaller.h"
#include "../daemon/global.h"

#include <QCoreApplication>

CommandCaller::CommandCaller( QString exchange, QByteArray command, QObject *parent )
    : QLocalSocket( parent )
{
    // make lowercase
    exchange = exchange.toLower();

    // parse the first argument as the daemon target
    if ( exchange == "binance" ) exchange = BINANCE_SUBPATH;
    else if ( exchange == "bittrex" ) exchange = BITTREX_SUBPATH;
    else if ( exchange == "poloniex" ) exchange = POLONIEX_SUBPATH;
    else
    {
        qDebug() << "error: an incorrect exchange was given, use `trader-cli <bittrex|poloniex|binance> <command>`";
        return;
    }

    QString path = Global::getIPCPath( exchange );
    QLocalSocket::connectToServer( path );
    QLocalSocket::waitForConnected();

    if ( state() == QLocalSocket::ConnectedState )
    {
        QLocalSocket::write( command );
        QLocalSocket::flush();
    }
    else
    {
        qDebug() << "error: failed to send command:" << QLocalSocket::errorString();
    }
}

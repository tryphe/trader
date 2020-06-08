#include "commandlistener.h"
#include "global.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>

CommandListener::CommandListener( QObject *parent )
    : QLocalServer( parent )
{
    QString path = Global::getIPCPath();

    // remove socket path if it exists
    if ( QFile::exists( path ) )
    {
        QFile f( path );
        f.remove();
    }

    // ensure we can listen
    if ( !QLocalServer::listen( path ) )
    {
        kDebug() << "[CommandListener] failed to listen at" << path;
        return;
    }

    connect( this, &QLocalServer::newConnection, this, &CommandListener::handleNewConnection );
    kDebug() << "[CommandListener] listening at" << path;
}

CommandListener::~CommandListener()
{
    QLocalServer::close();

    QList<LocalClient*> list = m_users.values();
    while ( !list.isEmpty() )
        delete list.takeFirst(); //->deleteLater();
}

void CommandListener::handleNewConnection()
{
    LocalClient *next = new LocalClient( QLocalServer::nextPendingConnection() );
    m_users += next;

    connect( next, &LocalClient::disconnected, this, &CommandListener::handleDisconnect );
    connect( next, &LocalClient::readyRead, this, &CommandListener::handleReadyRead );
    //kDebug() << "[CommandListener] socket connected";
}

void CommandListener::handleDisconnect( LocalClient *sck )
{
    m_users.remove( sck );
    sck->deleteLater();
    //kDebug() << "[CommandListener] socket disconnected";
}

void CommandListener::handleReadyRead( LocalClient *sck )
{
    QString data = sck->getSocketData();
    emit gotDataChunk( data );
    //kDebug() << "[CommandListener] " << data;
}


LocalClient::LocalClient( QLocalSocket *sck, QObject *parent )
    : QObject( parent ),
      m_sck( sck )
{
    connect( m_sck, &QLocalSocket::disconnected, this, &LocalClient::handleDisconnected );
    connect( m_sck, &QLocalSocket::readyRead, this, &LocalClient::handleReadyRead );
    //kDebug() << "[LocalClient()]";
}

LocalClient::~LocalClient()
{
    m_sck->deleteLater();
}

void LocalClient::handleDisconnected()
{
    emit disconnected( this );
}

void LocalClient::handleReadyRead()
{
    emit readyRead( this );
}

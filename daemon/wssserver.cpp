#include "wssserver.h"
#include "global.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QSslConfiguration>
#include <QSslCertificate>
#include <QSslKey>
#include <QUrl>
#include <QFile>
#include <QRandomGenerator>
#include <QDataStream>

#include <QDebug>

WSSServer::WSSServer()
    : QWebSocketServer( "WSS Server", QWebSocketServer::SecureMode )
{
    // read session token
    if ( !buildSessionToken() )
        return;

    // read self-signed cert (copied from qt example)
    QFile certFile( ":/x509.cert" );
    QFile keyFile( ":/x509.key" );
    certFile.open( QIODevice::ReadOnly );
    keyFile.open( QIODevice::ReadOnly );
    QSslCertificate certificate( &certFile, QSsl::Pem );
    QSslKey sslKey( &keyFile, QSsl::Rsa, QSsl::Pem ) ;
    certFile.close();
    keyFile.close();

    // load default ssl config
    QSslConfiguration ssl_config = QSslConfiguration::defaultConfiguration();
    ssl_config.setLocalCertificate( certificate );
    ssl_config.setPrivateKey( sslKey );
    ssl_config.setPeerVerifyMode( QSslSocket::VerifyNone );
    setSslConfiguration( ssl_config );

    // assert TLS version
    assert( ssl_config.protocol() == QSslConfiguration::defaultConfiguration().protocol() );

    // choose bind string or wildcard interface
    QHostAddress host;
    if ( QString( WSS_BIND ).size() > 0 )
        host = QHostAddress( WSS_BIND );
    else
        host = QHostAddress::WSS_ADDRESS;

    if ( !listen( host, WSS_PORT ) )
    {
        kDebug() << "[WSS Server] error: couldn't listen on" << host.toString();
        return;
    }

    kDebug() << "[WSS Server] listening on" << serverUrl().toString();
    connect( this, &QWebSocketServer::newConnection, this, &WSSServer::handleNewConnection );
}

WSSServer::~WSSServer()
{
    close();
    while ( m_clients.size() > 0 )
        remove( *m_clients.begin() );
}

bool WSSServer::buildSessionToken()
{
    QFile token_file( ":/session.token" );
    bool is_open = token_file.open( QFile::ReadOnly );

    if ( !is_open || token_file.size() == 0 )
    {
        kDebug() << "local error: couldn't open /session.token";
        return false;
    }

    // assemble the token and give a response
    session_base_token = token_file.readAll();

    return true;
}

void WSSServer::handleNewConnection()
{
    QWebSocket *const &sck = nextPendingConnection();

    // check for bad peer
    QString peer_str = sck->peerAddress().toString();
    quint32 dos_rating = m_rating[ peer_str ]++;
    if ( dos_rating > 100 )
    {
        remove( sck );
        return;
    }

    kDebug() << QString( "[WSS %1:%2] connected" )
                   .arg( peer_str )
                   .arg( sck->peerPort() );

    connect( sck, &QWebSocket::textMessageReceived, this, &WSSServer::processTextMessage );
    connect( sck, &QWebSocket::binaryFrameReceived, this, &WSSServer::processBinaryFrame );
    connect( sck, &QWebSocket::disconnected, this, &WSSServer::socketDisconnected );

    // construct 16 byte challenge
    QByteArray challenge;
    QDataStream stream( &challenge, QIODevice::WriteOnly );

    stream << QDateTime::currentMSecsSinceEpoch();
    stream << QRandomGenerator::global()->generate64();

    // convert to base64
    challenge = challenge.toBase64();

    m_clients += sck;
    m_userchallenge[ sck ] = challenge;
    m_authed[ sck ] = false;
    sck->sendTextMessage( challenge );
}

void WSSServer::processTextMessage( QString message )
{
    QWebSocket *const &sck = qobject_cast<QWebSocket *>(sender());

    if ( !sck )
        return;

    if ( m_authed[ sck ] == false )
    {
        QString token = QCryptographicHash::hash( m_userchallenge[ sck ] + session_base_token, QCryptographicHash::RealSha3_512 ).toBase64();

        if ( message == token )
        {
            kDebug() << QString( "[WSS %1:%2] challenge success" )
                           .arg( sck->peerAddress().toString() )
                           .arg( sck->peerPort() );
            m_authed[ sck ] = true;
            m_rating[ sck->peerAddress().toString() ] = 0; // reset rating to 0
            sck->sendTextMessage( "success" );

            // route new user messages to the engine
            disconnect( sck, &QWebSocket::textMessageReceived, this, &WSSServer::processTextMessage );
            connect( sck, &QWebSocket::textMessageReceived, this, &WSSServer::newUserMessage );
        }
        else
        {
            kDebug() << QString( "[WSS %1:%2] challenge failed" )
                           .arg( sck->peerAddress().toString() )
                           .arg( sck->peerPort() );
            remove( sck );
        }

        return;
    }
 }

void WSSServer::processBinaryFrame( const QByteArray &, bool )
{
    QWebSocket *const &sck = qobject_cast<QWebSocket *>(sender());

    if ( !sck )
        return;

    remove( sck );
}

void WSSServer::socketDisconnected()
{
    QWebSocket *const &sck = qobject_cast<QWebSocket *>(sender());

    if ( !sck )
        return;

    remove( sck );
}

void WSSServer::handleEngineMessage( QString &str )
{
    // send update stream out via wss
    for ( QMap<QWebSocket*,bool>::const_iterator i = m_authed.begin(); i != m_authed.end(); i++ )
    {
        QWebSocket *const &sck = i.key();
        const bool authed = i.value();

        if ( authed )
        {
            sck->sendTextMessage( str );
        }
    }
}

void WSSServer::remove( QWebSocket *const &sck )
{
    sck->abort();

    m_clients.remove( sck );
    m_authed.remove( sck );
    m_userchallenge.remove( sck );
    sck->deleteLater();
}

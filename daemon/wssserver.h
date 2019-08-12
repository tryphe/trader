#ifndef WSSSERVER_H
#define WSSSERVER_H

#include <QWebSocketServer>
#include <QSslError>
#include <QSet>


class QWebSocket;

class WSSServer : public QWebSocketServer
{
    Q_OBJECT

public:
    explicit WSSServer();
    ~WSSServer();

signals:
    void newUserMessage( const QString &msg );

public slots:
    void handleEngineMessage( QString &str );

private slots:
    void handleNewConnection();
    void processTextMessage( QString message );
    void processBinaryFrame( const QByteArray &frame, bool isLastFrame );
    void socketDisconnected();

private:
    void remove( QWebSocket * const &sck );
    bool buildSessionToken();

    QSet<QWebSocket*> m_clients;
    QMap<QWebSocket*,bool> m_authed;
    QMap<QWebSocket*,QByteArray> m_userchallenge;
    QMap<QString,quint32> m_rating;

    QByteArray session_base_token; // res/session.token file, it's the hash of keydefs.h
};

#endif //WSSSERVER_H

#ifndef COMMANDLISTENER_H
#define COMMANDLISTENER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QSet>
#include <QLocalServer>

class QLocalSocket;

//
// LocalClient, a signal wrapper around QLocalSocket
//
class LocalClient : public QObject
{
    Q_OBJECT
public:
    explicit LocalClient( QLocalSocket *sck, QObject *parent = nullptr );\
    ~LocalClient();

    QByteArray getSocketData() const;

signals:
    void disconnected( LocalClient *sck );
    void readyRead( LocalClient *sck );

public slots:
    void handleDisconnected();
    void handleReadyRead();

private:
    QLocalSocket *m_sck;
};

//
// CommandListener, an IPC server to listen for commands
//
class CommandListener : public QLocalServer
{
    Q_OBJECT
public:
    explicit CommandListener( QObject *parent = nullptr );
    ~CommandListener();

signals:
    void gotDataChunk( QString &s );

public slots:
    void handleNewConnection();
    void handleDisconnect( LocalClient *sck );
    void handleReadyRead( LocalClient *sck );

private:
    QSet<LocalClient*> m_users;
};

#endif // COMMANDLISTENER_H

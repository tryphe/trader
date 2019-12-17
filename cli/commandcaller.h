#ifndef COMMANDCALLER_H
#define COMMANDCALLER_H

#include <QByteArray>
#include <QLocalSocket>

class CommandCaller : public QLocalSocket
{
    Q_OBJECT

public:
    explicit CommandCaller( const QByteArray &command, QObject *parent = nullptr );
};

#endif // COMMANDCALLER_H

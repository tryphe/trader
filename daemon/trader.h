#ifndef TREXTRADER_H
#define TREXTRADER_H

#include <QObject>

#include "global.h"

class CommandRunner;
class CommandListener;
class FallbackListener;
class WSSServer;

class Stats;
class Engine;
class REST_OBJECT;

class Trader : public QObject
{
    Q_OBJECT

public:
    explicit Trader( QObject *parent = nullptr );
    ~Trader();

public slots:
    void handleExitSignal();

private:
    CommandRunner *runner{ nullptr };
    CommandListener *listener{ nullptr };
    FallbackListener *listener_fallback{ nullptr };
    WSSServer *wss_server{ nullptr };

    Stats *stats{ nullptr };
    Engine *engine{ nullptr };
    REST_OBJECT *rest{ nullptr };
};

#endif // TREXTRADER_H

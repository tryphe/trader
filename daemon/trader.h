#ifndef TREXTRADER_H
#define TREXTRADER_H

#include <QObject>

#include "global.h"

class CommandRunner;
class CommandListener;
class FallbackListener;
class WSSServer;

class AlphaTracker;
class Spruce;
class Engine;
class TrexREST;
class BncREST;
class PoloREST;

class Trader : public QObject
{
    Q_OBJECT

public:
    explicit Trader( QObject *parent = nullptr );
    ~Trader();

public slots:
    void handleCommand( QString &s );
    void handleExitSignal();

private:
    CommandListener *command_listener{ nullptr };
    CommandRunner *command_runner_trex{ nullptr };
    CommandRunner *command_runner_bnc{ nullptr };
    CommandRunner *command_runner_polo{ nullptr };

    AlphaTracker *alpha{ nullptr };
    Spruce *spruce{ nullptr };

    Engine* engine_trex{ nullptr };
    Engine* engine_bnc{ nullptr };
    Engine* engine_polo{ nullptr };

    TrexREST *rest_trex{ nullptr };
    BncREST *rest_bnc{ nullptr };
    PoloREST *rest_polo{ nullptr };
};

#endif // TREXTRADER_H

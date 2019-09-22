#include "trader.h"
#include "bncrest.h"
#include "stats.h"
#include "position.h"
#include "engine.h"
#include "engine_test.h"
#include "coinamount_test.h"
#include "trexrest.h"
#include "polorest.h"
#include "bncrest.h"
#include "ssl_policy.h"
#include "fallbacklistener.h"
#include "commandlistener.h"
#include "commandrunner.h"
#include "wssserver.h"

#include <QByteArray>
#include <QTimer>
#include <QThread>
#include <QCoreApplication>


Trader::Trader( QObject *parent )
  : QObject( parent )
{
    // ssl hacks
    GlobalSsl::enableSecureSsl();

    // engine init
    engine = new Engine();

    rest = new REST_OBJECT( engine );
    stats = rest->stats = new Stats( engine, rest );

    engine->setRest( rest );
    engine->setStats( stats );

    // runtime tests
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    CoinAmountTest coin_test; coin_test.test();
    EngineTest engine_test; engine_test.test( engine );

    qint64 t1 = QDateTime::currentMSecsSinceEpoch();
    kDebug() << "[Trader] Tests passed in" << t1 - t0 << "ms.";

    // print build info
    kDebug() << "[Trader] Startup success." << Global::getBuildString();

    // create command runner
    runner = new CommandRunner( engine, rest, stats );
    connect( runner, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );

    // open IPC command listener
    listener = new CommandListener();
    connect( listener, &CommandListener::gotDataChunk, runner, &CommandRunner::runCommandChunk );
    connect( engine, &Engine::gotUserCommandChunk, runner, &CommandRunner::runCommandChunk );

    engine->loadSettings();

    // open fallback listener that uses a plain file, useful for copying a 'setorder' dump into a file
    listener_fallback = new FallbackListener();
    connect( listener_fallback, &FallbackListener::gotDataChunk, runner, &CommandRunner::runCommandChunk );

    // open wss server
#ifdef WSS_INTERFACE
    wss_server = new WSSServer();
    connect( engine, &Engine::newEngineMessage, wss_server, &WSSServer::handleEngineMessage );
    connect( wss_server, &WSSServer::newUserMessage, engine, &Engine::handleUserMessage );
#endif

    // tests passed. start rest interface which powers the engine
    rest->init();
}

Trader::~Trader()
{
    delete listener;
    delete runner;
    delete listener_fallback;
    if ( wss_server ) delete wss_server;

    delete engine;
    delete rest;
    delete stats;

    QCoreApplication::processEvents( QEventLoop::AllEvents, 10000 );

    kDebug() << "[Trader] done.";
}

void Trader::handleExitSignal()
{
    kDebug() << "[Trader] Got exit signal, shutting down...";
    this->~Trader();
}

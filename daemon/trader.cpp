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
#include "global.h"
#include "alphatracker.h"
#include "spruce.h"
#include "trexrest.h"
#include "bncrest.h"
#include "polorest.h"

#include <QByteArray>
#include <QTimer>
#include <QThread>
#include <QCoreApplication>


Trader::Trader( QObject *parent )
  : QObject( parent )
{
    bool bittrex = false,
         binance = false,
         poloniex = false;

    // ssl hacks
    GlobalSsl::enableSecureSsl();

    alpha = new AlphaTracker();
    spruce = new Spruce();

    // engine init
#ifdef BITTREX_ENABLED
    engine_trex = new Engine( ENGINE_BITTREX );
    rest_trex = new TrexREST( engine_trex );
    engine_trex->rest_trex = rest_trex;
    engine_trex->alpha = alpha;
    engine_trex->spruce = spruce;

    bittrex = true;
#endif

#ifdef BINANCE_ENABLED
    engine_bnc = new Engine( ENGINE_BINANCE );
    rest_bnc = new BncREST( engine_bnc );
    engine_bnc->rest_bnc = rest_bnc;
    engine_bnc->alpha = alpha;
    engine_bnc->spruce = spruce;

    binance = true;
#endif

#ifdef POLONIEX_ENABLED
    engine_polo = new Engine( ENGINE_POLONIEX );
    rest_polo = new PoloREST( engine_polo );
    engine_polo->rest_polo = rest_polo;
    engine_polo->alpha = alpha;
    engine_polo->spruce = spruce;

    poloniex = true;
#endif

    // runtime tests
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    CoinAmountTest coin_test; coin_test.test();
    EngineTest engine_test;
    if ( bittrex  ) engine_test.test( engine_trex );
    if ( binance  ) engine_test.test( engine_bnc );
    if ( poloniex ) engine_test.test( engine_polo );

    qint64 t1 = QDateTime::currentMSecsSinceEpoch();
    kDebug() << "[Trader] Tests passed in" << t1 - t0 << "ms.";

    // print build info
    kDebug() << "[Trader] Startup success." << Global::getBuildString();

    // create command runner
    if ( bittrex )
    {
        command_runner_trex = new CommandRunner( ENGINE_BITTREX, engine_trex, rest_trex );
        connect( command_runner_trex, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
    }
    if ( binance )
    {
        command_runner_bnc = new CommandRunner( ENGINE_BINANCE, engine_bnc, rest_bnc );
        connect( command_runner_bnc,  &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
    }
    if ( poloniex )
    {
        command_runner_polo = new CommandRunner( ENGINE_POLONIEX, engine_polo, rest_polo );
        connect( command_runner_polo, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
    }

    // open IPC command listener
    command_listener = new CommandListener();
    connect( command_listener, &CommandListener::gotDataChunk, this, &Trader::handleCommand );

    // TODO: disabled for now (wss input only)
    //connect( engine, &Engine::gotUserCommandChunk, runner, &CommandRunner::runCommandChunk );

    // open fallback listener that uses a plain file, useful for copying a 'setorder' dump into a file
//    listener_fallback = new FallbackListener();
//    connect( listener_fallback, &FallbackListener::gotDataChunk, runner, &CommandRunner::runCommandChunk );

    // tests passed. start rest, load settings and stats, initialize api keys
    if ( bittrex )  rest_trex->init();
    if ( binance )  rest_bnc->init();
    if ( poloniex ) rest_polo->init();
}

Trader::~Trader()
{
    delete engine_trex;
    delete engine_bnc;
    delete engine_polo;
    delete rest_trex;
    delete rest_bnc;
    delete rest_polo;
    delete command_runner_trex;
    delete command_runner_bnc;
    delete command_runner_polo;
    delete command_listener;
    delete alpha;

    QCoreApplication::processEvents( QEventLoop::AllEvents, 10000 );

    kDebug() << "[Trader] done.";
}

void Trader::handleCommand( QString &s )
{
    kDebug() << "[Trader] got command:" << s;

    // TODO: choose command listener
}

void Trader::handleExitSignal()
{
    kDebug() << "[Trader] Got exit signal, shutting down...";
    this->~Trader();
}

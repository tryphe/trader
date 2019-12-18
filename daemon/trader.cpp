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
#include "spruceoverseer.h"
#include "trexrest.h"
#include "bncrest.h"
#include "polorest.h"

#include <QByteArray>
#include <QTimer>
#include <QThread>
#include <QCoreApplication>
#include <QNetworkAccessManager>


Trader::Trader( QObject *parent )
  : QObject( parent )
{
    bool bittrex = false,
         binance = false,
         poloniex = false;

    // ssl hacks
    GlobalSsl::enableSecureSsl();

    // create spruce and spruceOverseer
    alpha = new AlphaTracker();
    spruce = new Spruce();
    spruce_overseer = new SpruceOverseer();
    spruce_overseer->alpha = alpha;
    spruce_overseer->spruce = spruce;

    QNetworkAccessManager *nam = new QNetworkAccessManager();

    // engine init
#ifdef BITTREX_ENABLED
    engine_trex = new Engine( ENGINE_BITTREX );
    rest_trex = new TrexREST( engine_trex, nam );
    engine_trex->rest_trex = rest_trex;
    engine_trex->alpha = alpha;
    engine_trex->spruce = spruce;

    spruce_overseer->engine_map.insert( ENGINE_BITTREX, engine_trex );

    bittrex = true;
#endif

#ifdef BINANCE_ENABLED
    engine_bnc = new Engine( ENGINE_BINANCE );
    rest_bnc = new BncREST( engine_bnc, nam );
    engine_bnc->rest_bnc = rest_bnc;
    engine_bnc->alpha = alpha;
    engine_bnc->spruce = spruce;

    spruce_overseer->engine_map.insert( ENGINE_BINANCE, engine_bnc );

    binance = true;
#endif

#ifdef POLONIEX_ENABLED
    engine_polo = new Engine( ENGINE_POLONIEX );
    rest_polo = new PoloREST( engine_polo, nam );
    engine_polo->rest_polo = rest_polo;
    engine_polo->alpha = alpha;
    engine_polo->spruce = spruce;

    spruce_overseer->engine_map.insert( ENGINE_POLONIEX, engine_polo );

    poloniex = true;
#endif

    // runtime tests
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    CoinAmountTest coin_test;
    coin_test.test();

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
        command_runner_trex->spruce_overseer = spruce_overseer;
        connect( command_runner_trex, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
        connect( engine_trex, &Engine::gotUserCommandChunk, command_runner_trex, &CommandRunner::runCommandChunk );
    }
    if ( binance )
    {
        command_runner_bnc = new CommandRunner( ENGINE_BINANCE, engine_bnc, rest_bnc );
        command_runner_bnc->spruce_overseer = spruce_overseer;
        connect( command_runner_bnc,  &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
        connect( engine_bnc, &Engine::gotUserCommandChunk, command_runner_bnc, &CommandRunner::runCommandChunk );
    }
    if ( poloniex )
    {
        command_runner_polo = new CommandRunner( ENGINE_POLONIEX, engine_polo, rest_polo );
        command_runner_polo->spruce_overseer = spruce_overseer;
        connect( command_runner_polo, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
        connect( engine_polo, &Engine::gotUserCommandChunk, command_runner_polo, &CommandRunner::runCommandChunk );
    }

    // open IPC command listener
    command_listener = new CommandListener();
    connect( command_listener, &CommandListener::gotDataChunk, this, &Trader::handleCommand );

    // open fallback listener that uses a plain file, useful for copying a 'setorder' dump into a file
//    listener_fallback = new FallbackListener();
//    connect( listener_fallback, &FallbackListener::gotDataChunk, runner, &CommandRunner::runCommandChunk );

    // tests passed. start rest, load settings and stats, initialize api keys
    if ( bittrex )  rest_trex->init();
    if ( binance )  rest_bnc->init();
    if ( poloniex ) rest_polo->init();

    spruce_overseer->loadSettings();
    spruce_overseer->loadStats();
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
    //kDebug() << "[Trader] got command:" << s;

    if ( s.startsWith( QString( "bittrex " ), Qt::CaseInsensitive ) )
    {
        s = s.mid( 8 );
        command_runner_trex->runCommandChunk( s );
    }
    else if ( s.startsWith( QString( "binance " ), Qt::CaseInsensitive ) )
    {
        s = s.mid( 8 );
        command_runner_bnc->runCommandChunk( s );
    }
    else if ( s.startsWith( QString( "poloniex " ), Qt::CaseInsensitive ) )
    {
        s = s.mid( 9 );
        command_runner_polo->runCommandChunk( s );
    }
    else
    {
        kDebug() << "[Trader] bad exchange prefix:" << s;
        return;
    }
}

void Trader::handleExitSignal()
{
    kDebug() << "[Trader] Got exit signal, shutting down...";
    this->~Trader();
}

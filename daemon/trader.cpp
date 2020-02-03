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
#include "wavesrest.h"
#include "ssl_policy.h"
#include "fallbacklistener.h"
#include "commandlistener.h"
#include "commandrunner.h"
#include "global.h"
#include "alphatracker.h"
#include "spruce.h"
#include "spruceoverseer.h"
#include "wavesutil_test.h"
#include "wavesaccount_test.h"
#include "../qbase58/qbase58_test.h"

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
         poloniex = false,
         waves = false;

    // ssl hacks
    GlobalSsl::enableSecureSsl();

    // create spruce and spruceOverseer
    alpha = new AlphaTracker();
    spruce = new Spruce();
    spruce_overseer = new SpruceOverseer( spruce );
    spruce_overseer->alpha = alpha;

    nam = new QNetworkAccessManager();

    // engine init
#ifdef BITTREX_ENABLED
    engine_trex = new Engine( ENGINE_BITTREX );
    rest_trex = new TrexREST( engine_trex, nam );
    engine_trex->alpha = alpha;
    engine_trex->spruce = spruce;

    spruce_overseer->engine_map.insert( ENGINE_BITTREX, engine_trex );

    bittrex = true;
#endif

#ifdef BINANCE_ENABLED
    engine_bnc = new Engine( ENGINE_BINANCE );
    rest_bnc = new BncREST( engine_bnc, nam );
    engine_bnc->alpha = alpha;
    engine_bnc->spruce = spruce;

    spruce_overseer->engine_map.insert( ENGINE_BINANCE, engine_bnc );

    binance = true;
#endif

#ifdef POLONIEX_ENABLED
    engine_polo = new Engine( ENGINE_POLONIEX );
    rest_polo = new PoloREST( engine_polo, nam );
    engine_polo->alpha = alpha;
    engine_polo->spruce = spruce;

    spruce_overseer->engine_map.insert( ENGINE_POLONIEX, engine_polo );

    poloniex = true;
#endif

#ifdef WAVES_ENABLED
    engine_waves = new Engine( ENGINE_WAVES );
    rest_waves = new WavesREST( engine_waves, nam );
    engine_waves->alpha = alpha;
    engine_waves->spruce = spruce;

    spruce_overseer->engine_map.insert( ENGINE_WAVES, engine_waves );

    waves = true;
#endif

    QVector<BaseREST*> rest_arr;
    rest_arr += rest_trex;
    rest_arr += rest_bnc;
    rest_arr += rest_polo;
    rest_arr += rest_waves;

    // create command runner
    if ( bittrex )
    {
        engine_trex->rest_arr = rest_arr;
        command_runner_trex = new CommandRunner( ENGINE_BITTREX, engine_trex, rest_arr );
        command_runner_trex->spruce_overseer = spruce_overseer;
        connect( command_runner_trex, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
        connect( engine_trex, &Engine::gotUserCommandChunk, command_runner_trex, &CommandRunner::runCommandChunk );
    }
    if ( binance )
    {
        engine_bnc->rest_arr = rest_arr;
        command_runner_bnc = new CommandRunner( ENGINE_BINANCE, engine_bnc, rest_arr );
        command_runner_bnc->spruce_overseer = spruce_overseer;
        connect( command_runner_bnc,  &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
        connect( engine_bnc, &Engine::gotUserCommandChunk, command_runner_bnc, &CommandRunner::runCommandChunk );
    }
    if ( poloniex )
    {
        engine_polo->rest_arr = rest_arr;
        command_runner_polo = new CommandRunner( ENGINE_POLONIEX, engine_polo, rest_arr );
        command_runner_polo->spruce_overseer = spruce_overseer;
        connect( command_runner_polo, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
        connect( engine_polo, &Engine::gotUserCommandChunk, command_runner_polo, &CommandRunner::runCommandChunk );
    }
    if ( waves )
    {
        engine_waves->rest_arr = rest_arr;
        command_runner_waves = new CommandRunner( ENGINE_WAVES, engine_waves, rest_arr );
        command_runner_waves->spruce_overseer = spruce_overseer;
        connect( command_runner_waves, &CommandRunner::exitSignal, this, &Trader::handleExitSignal );
        connect( engine_waves, &Engine::gotUserCommandChunk, command_runner_waves, &CommandRunner::runCommandChunk );
    }

    // runtime tests
    qint64 t0 = QDateTime::currentMSecsSinceEpoch();

    QBase58Test qbase58_test;
    qbase58_test.test();

    WavesUtilTest wavesutil_test;
    wavesutil_test.test();

    WavesAccountTest wavesaccount_test;
    wavesaccount_test.test();

    CoinAmountTest coin_test;
    coin_test.test();

    EngineTest engine_test;
    if ( bittrex  ) engine_test.test( engine_trex );
    if ( binance  ) engine_test.test( engine_bnc );
    if ( poloniex ) engine_test.test( engine_polo );
    if ( waves )    engine_test.test( engine_waves );

    qint64 t1 = QDateTime::currentMSecsSinceEpoch();
    kDebug() << "[Trader] Tests passed in" << t1 - t0 << "ms.";

    // print build info
    kDebug() << "[Trader] Startup success." << Global::getBuildString();


    // connect spruce_overseer to a command runner that isn't null
    CommandRunner *command_runner = command_runner_trex != nullptr  ? command_runner_trex :
                                    command_runner_bnc != nullptr   ? command_runner_bnc :
                                    command_runner_polo != nullptr  ? command_runner_polo :
                                    command_runner_waves != nullptr ? command_runner_waves :
                                                                     nullptr;

    if ( command_runner )
        connect( spruce_overseer, &SpruceOverseer::gotUserCommandChunk, command_runner, &CommandRunner::runCommandChunk );

    // open IPC command listener
    command_listener = new CommandListener();
    connect( command_listener, &CommandListener::gotDataChunk, this, &Trader::handleCommand );

    // open fallback listener that uses a plain file, useful for copying a 'setorder' dump into a file
//    listener_fallback = new FallbackListener();
//    connect( listener_fallback, &FallbackListener::gotDataChunk, runner, &CommandRunner::runCommandChunk );

    // tests passed. start rest, load settings and stats, initialize api keys
    for ( int i = 0; i < rest_arr.size(); i++ )
        if ( rest_arr.at( i ) != nullptr )
            rest_arr.at( i )->init();

    spruce_overseer->loadSettings();
    spruce_overseer->loadStats();
}

Trader::~Trader()
{
    delete engine_trex;
    delete engine_bnc;
    delete engine_polo;
    delete engine_waves;
    delete rest_trex;
    delete rest_bnc;
    delete rest_polo;
    delete rest_waves;
    delete command_runner_trex;
    delete command_runner_bnc;
    delete command_runner_polo;
    delete command_runner_waves;
    delete command_listener;
    delete alpha;

    QCoreApplication::processEvents( QEventLoop::AllEvents, 10000 );

    // force nam to close
    nam->thread()->exit();
    delete nam;
    nam = nullptr;

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
    else if ( s.startsWith( QString( "waves " ), Qt::CaseInsensitive ) )
    {
        s = s.mid( 6 );
        command_runner_waves->runCommandChunk( s );
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

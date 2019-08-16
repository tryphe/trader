#include "commandrunner.h"
#include "global.h"
#include "engine.h"
#include "enginesettings.h"
#include "trexrest.h"
#include "polorest.h"
#include "bncrest.h"
#include "stats.h"
#include "positionman.h"

#include <functional>

#include <QString>
#include <QMap>
#include <QQueue>

CommandRunner::CommandRunner( Engine *_e, REST_OBJECT *_rest, Stats *_stats, QObject *parent )
    : QObject( parent ),
      engine( _e ),
      rest( _rest ),
      stats( _stats )
{
    // map strings to functions
    using std::placeholders::_1;
    command_map.insert( "getbalances", std::bind( &CommandRunner::command_getbalances, this, _1 ) );
    command_map.insert( "getlastprices", std::bind( &CommandRunner::command_getlastprices, this, _1 ) );
    command_map.insert( "getbuyselltotal", std::bind( &CommandRunner::command_getbuyselltotal, this, _1 ) );
    command_map.insert( "cancelall", std::bind( &CommandRunner::command_cancelall, this, _1 ) );
    command_map.insert( "cancellocal", std::bind( &CommandRunner::command_cancellocal, this, _1 ) );
    //command_map.insert( "cancelorder", std::bind( &CommandRunner::command_cancelorder, this, _1 ) );
    command_map.insert( "cancelhighest", std::bind( &CommandRunner::command_cancelhighest, this, _1 ) );
    command_map.insert( "cancellowest", std::bind( &CommandRunner::command_cancellowest, this, _1 ) );
    command_map.insert( "getorders", std::bind( &CommandRunner::command_getorders, this, _1 ) );
    command_map.insert( "getpositions", std::bind( &CommandRunner::command_getpositions, this, _1 ) );
    command_map.insert( "getordersbyindex", std::bind( &CommandRunner::command_getordersbyindex, this, _1 ) );
    command_map.insert( "setorder", std::bind( &CommandRunner::command_setorder, this, _1 ) );
    command_map.insert( "setordermin", std::bind( &CommandRunner::command_setordermin, this, _1 ) );
    command_map.insert( "setordermax", std::bind( &CommandRunner::command_setordermax, this, _1 ) );
    command_map.insert( "setorderdc", std::bind( &CommandRunner::command_setorderdc, this, _1 ) );
    command_map.insert( "setorderdcnice", std::bind( &CommandRunner::command_setorderdcnice, this, _1 ) );
    command_map.insert( "setorderlandmarkthresh", std::bind( &CommandRunner::command_setorderlandmarkthresh, this, _1 ) );
    command_map.insert( "setorderlandmarkstart", std::bind( &CommandRunner::command_setorderlandmarkstart, this, _1 ) );
    command_map.insert( "long", std::bind( &CommandRunner::command_long, this, _1 ) );
    command_map.insert( "longindex", std::bind( &CommandRunner::command_longindex, this, _1 ) );
    command_map.insert( "short", std::bind( &CommandRunner::command_short, this, _1 ) );
    command_map.insert( "shortindex", std::bind( &CommandRunner::command_shortindex, this, _1 ) );
    command_map.insert( "setcancelthresh", std::bind( &CommandRunner::command_setcancelthresh, this, _1 ) );
    command_map.insert( "setkeyandsecret", std::bind( &CommandRunner::command_setkeyandsecret, this, _1 ) );
    command_map.insert( "getvolume", std::bind( &CommandRunner::command_getvolume, this, _1 ) );
    command_map.insert( "getdailyvolume", std::bind( &CommandRunner::command_getdailyvolume, this, _1 ) );
    command_map.insert( "getdailyprofit", std::bind( &CommandRunner::command_getdailyprofit, this, _1 ) );
    command_map.insert( "getdailyfills", std::bind( &CommandRunner::command_getdailyfills, this, _1 ) );
    command_map.insert( "getprofit", std::bind( &CommandRunner::command_getprofit, this, _1 ) );
    command_map.insert( "getmarketprofit", std::bind( &CommandRunner::command_getmarketprofit, this, _1 ) );
    command_map.insert( "getdailymarketprofit", std::bind( &CommandRunner::command_getdailymarketprofit, this, _1 ) );
    command_map.insert( "getdailymarketvolume", std::bind( &CommandRunner::command_getdailymarketvolume, this, _1 ) );
    command_map.insert( "getdailymarketprofitvolume", std::bind( &CommandRunner::command_getdailymarketprofitvolume, this, _1 ) );
    command_map.insert( "getmarketprofitvolume", std::bind( &CommandRunner::command_getmarketprofitvolume, this, _1 ) );
    command_map.insert( "getmarketprofitfills", std::bind( &CommandRunner::command_getmarketprofitfills, this, _1 ) );
    command_map.insert( "getdailymarketprofitriskreward", std::bind( &CommandRunner::command_getdailymarketprofitriskreward, this, _1 ) );
    command_map.insert( "getmarketprofitriskreward", std::bind( &CommandRunner::command_getmarketprofitriskreward, this, _1 ) );
    command_map.insert( "getfills", std::bind( &CommandRunner::command_getfills, this, _1 ) );
    command_map.insert( "getshortlong", std::bind( &CommandRunner::command_getshortlong, this, _1 ) );
    command_map.insert( "gethibuylosell", std::bind( &CommandRunner::command_gethibuylosell, this, _1 ) );
    command_map.insert( "setmarketsettings", std::bind( &CommandRunner::command_setmarketsettings, this, _1 ) );
    command_map.insert( "setmarketoffset", std::bind( &CommandRunner::command_setmarketoffset, this, _1 ) );
    command_map.insert( "setmarketsentiment", std::bind( &CommandRunner::command_setmarketsentiment, this, _1 ) );
    command_map.insert( "setnaminterval", std::bind( &CommandRunner::command_setnaminterval, this, _1 ) );
    command_map.insert( "setbookinterval", std::bind( &CommandRunner::command_setbookinterval, this, _1 ) );
    command_map.insert( "settickerinterval", std::bind( &CommandRunner::command_settickerinterval, this, _1 ) );
    command_map.insert( "setgracetimelimit", std::bind( &CommandRunner::command_setgracetimelimit, this, _1 ) );
    command_map.insert( "setcheckinterval", std::bind( &CommandRunner::command_setcheckinterval, this, _1 ) );
    command_map.insert( "setdcinterval", std::bind( &CommandRunner::command_setdcinterval, this, _1 ) );
    command_map.insert( "setclearstrayorders", std::bind( &CommandRunner::command_setclearstrayorders, this, _1 ) );
    command_map.insert( "setclearstrayordersall", std::bind( &CommandRunner::command_setclearstrayordersall, this, _1 ) );
    command_map.insert( "setslippagecalculated", std::bind( &CommandRunner::command_setslippagecalculated, this, _1 ) );
    command_map.insert( "setadjustbuysell", std::bind( &CommandRunner::command_setadjustbuysell, this, _1 ) );
    command_map.insert( "setdcslippage", std::bind( &CommandRunner::command_setdcslippage, this, _1 ) );
    command_map.insert( "setorderbookstaletolerance", std::bind( &CommandRunner::command_setorderbookstaletolerance, this, _1 ) );
    command_map.insert( "setsafetydelaytime", std::bind( &CommandRunner::command_setsafetydelaytime, this, _1 ) );
    command_map.insert( "settickersafetydelaytime", std::bind( &CommandRunner::command_settickersafetydelaytime, this, _1 ) );
    command_map.insert( "setslippagestaletime", std::bind( &CommandRunner::command_setslippagestaletime, this, _1 ) );
    command_map.insert( "setqueuedcommandsmax", std::bind( &CommandRunner::command_setqueuedcommandsmax, this, _1 ) );
    command_map.insert( "setqueuedcommandsmaxdc", std::bind( &CommandRunner::command_setqueuedcommandsmaxdc, this, _1 ) );
    command_map.insert( "setsentcommandsmax", std::bind( &CommandRunner::command_setsentcommandsmax, this, _1 ) );
    command_map.insert( "settimeoutyield", std::bind( &CommandRunner::command_settimeoutyield, this, _1 ) );
    command_map.insert( "setrequesttimeout", std::bind( &CommandRunner::command_setrequesttimeout, this, _1 ) );
    command_map.insert( "setcanceltimeout", std::bind( &CommandRunner::command_setcanceltimeout, this, _1 ) );
    command_map.insert( "setslippagetimeout", std::bind( &CommandRunner::command_setslippagetimeout, this, _1 ) );
    command_map.insert( "setsprucebasecurrency", std::bind( &CommandRunner::command_setsprucebasecurrency, this, _1 ) );
    command_map.insert( "setspruceweight", std::bind( &CommandRunner::command_setspruceweight, this, _1 ) );
    command_map.insert( "setsprucestartnode", std::bind( &CommandRunner::command_setsprucestartnode, this, _1 ) );
    command_map.insert( "setspruceshortlongtotal", std::bind( &CommandRunner::command_setspruceshortlongtotal, this, _1 ) );
    command_map.insert( "getconfig", std::bind( &CommandRunner::command_getconfig, this, _1 ) );
    command_map.insert( "getinternal", std::bind( &CommandRunner::command_getinternal, this, _1 ) );
    command_map.insert( "setmaintenancetime", std::bind( &CommandRunner::command_setmaintenancetime, this, _1 ) );
    command_map.insert( "clearstratstats", std::bind( &CommandRunner::command_clearstratstats, this, _1 ) );
    command_map.insert( "clearallstats", std::bind( &CommandRunner::command_clearallstats, this, _1 ) );
    command_map.insert( "savemarket", std::bind( &CommandRunner::command_savemarket, this, _1 ) );
    command_map.insert( "savesettings", std::bind( &CommandRunner::command_savesettings, this, _1 ) );
    command_map.insert( "sendcommand", std::bind( &CommandRunner::command_sendcommand, this, _1 ) );
    command_map.insert( "setchatty", std::bind( &CommandRunner::command_setchatty, this, _1 ) );
    command_map.insert( "exit", std::bind( &CommandRunner::command_exit, this, _1 ) );
    command_map.insert( "stop", std::bind( &CommandRunner::command_exit, this, _1 ) );
    command_map.insert( "quit", std::bind( &CommandRunner::command_exit, this, _1 ) );

#if defined(EXCHANGE_BITTREX)
    command_map.insert( "sethistoryinterval", std::bind( &CommandRunner::command_sethistoryinterval, this, _1 ) );
#endif

    kDebug() << "[CommandRunner]";
}

CommandRunner::~CommandRunner()
{

}

void CommandRunner::runCommandChunk( QString &s )
{
    QQueue<QStringList> commands;
    QMap<QString, qint32> times_called; // count of commands called
    QMap<QString, qint32> positions_added; // count of positions set in each market

    QStringList lines = s.split( "\n" );
    while ( lines.size() > 0 )
    {
        QString line = lines.takeFirst();

        // preparse args so we can count the number of each command
        QStringList args = line.split( QChar( ' ' ) );

        QString command = args.first().toLower();

        // if command is empty, continue
        if ( command.isNull() || command.isEmpty() )
            continue;

        // record how many times it was called
        times_called[ command ]++;
        commands += args;
    }

    // early return
    if ( commands.isEmpty() )
        return;

    // parse all commands
    while ( !commands.isEmpty() )
    {
        QStringList args = commands.takeFirst();
        QString cmd = args.first();

        qint32 times = times_called.value( cmd );

        // if the command is unknown, print it until suppressed
        if ( !command_map.contains( cmd ) )
        {
            kDebug() << "[CommandRunner] unknown command:" << cmd;
            continue;
        }

        // do not leak key/secret into debug log
        if ( cmd.startsWith( "setkey" ) )
        {
            kDebug() << "[CommandRunner] running setkey...";
        }
        // be nice to the log
        else if ( times > 10 )
        {
            times_called[ cmd ] = 0; // don't print any more lines for this command
            kDebug() << QString( "[CommandRunner] running '%1' x%2" )
                            .arg( cmd )
                            .arg( times );
        }
        else if ( times > 0 )
        {
            kDebug() << "[CommandRunner] running:" << args.join( QChar( ' ' ) );
        }

        // if we set an order, increment positions_added
        if ( args.size() > 1 && cmd == "setorder" )
            positions_added[ args.value( 1 ) ]++;

        // run command
        std::function<void(QStringList&)> _func = command_map.value( cmd );
        _func( args );
    }

    // avoid addPosition() spam by logging stuff about 'setorder' here
    for ( QMap<QString, qint32>::const_iterator i = positions_added.begin(); i != positions_added.end(); i++ )
    {
        const QString &market = i.key();
        qint32 orders_set = i.value();

        kDebug() << QString( "[%1] processed %2 indices. total ping-pong indices: %3, active orders: %4 active onetime orders: %5" )
                        .arg( market )
                        .arg( orders_set )
                        .arg( engine->getMarketInfo( market ).position_index.size() )
                        .arg( engine->positions->getMarketOrderTotal( market ) )
                        .arg( engine->positions->getMarketOrderTotal( market, true ) );
    }
}

bool CommandRunner::checkArgs( const QStringList &args, qint32 expected_args_min, qint32 expected_args_max )
{
    expected_args_min++; // add one
    if ( expected_args_max < 0 )
        expected_args_max = expected_args_min;

    // check for expected arg count
    if ( args.size() < expected_args_min || args.size() > expected_args_max )
    {
        kDebug() << QString( "[CommandRunner] not enough args (%1)" )
                        .arg( expected_args_min );
        return false;
    }

    return true;
}

void CommandRunner::command_getbalances( QStringList &args )
{
    Q_UNUSED( args )
#if defined(EXCHANGE_BITTREX)
    rest->sendRequest( TREX_COMMAND_GET_BALANCES );
#elif defined(EXCHANGE_BINANCE)
    rest->sendRequest( BNC_COMMAND_GETBALANCES, "", nullptr, 5 );
#elif defined(EXCHANGE_POLONIEX)
    rest->sendRequest( POLO_COMMAND_GETBALANCES );
#endif
}

void CommandRunner::command_getlastprices( QStringList &args )
{
    Q_UNUSED( args )
    stats->printLastPrices();
}

void CommandRunner::command_getbuyselltotal( QStringList &args )
{
    Q_UNUSED( args )
    stats->printBuySellTotal();
}

void CommandRunner::command_cancelall( QStringList &args )
{
    engine->positions->cancelAll( args.value( 1 ) );
}

void CommandRunner::command_cancellocal( QStringList &args )
{
    engine->positions->cancelLocal( args.value( 1 ) );
}

//void CommandRunner::command_cancelorder( QStringList &args )
//{
//    engine->cancelOrderByPrice( args.value( 1 ), args.value( 2 ) );
//}

void CommandRunner::command_cancelhighest( QStringList &args )
{
    engine->positions->cancelHighest( args.value( 1 ) );
}

void CommandRunner::command_cancellowest( QStringList &args )
{
    engine->positions->cancelLowest( args.value( 1 ) );
}

void CommandRunner::command_getorders( QStringList &args )
{
    stats->printOrders( args.value( 1 ) );
}

void CommandRunner::command_getpositions( QStringList &args )
{
    stats->printPositions( args.value( 1 ) );
}

void CommandRunner::command_getordersbyindex( QStringList &args )
{
    stats->printOrdersByIndex( args.value( 1 ) );
}

void CommandRunner::command_setorder( QStringList &args )
{
    if ( !checkArgs( args, 6, 7 ) ) return;

    const QString &market = args.value( 1 );
    quint8 side = args.value( 2 ) == BUY ? SIDE_BUY :
                  args.value( 2 ) == SELL ? SIDE_SELL : 0;
    const QString &lo = args.value( 3 );
    const QString &hi = args.value( 4 );
    const QString &size = args.value( 5 );
    const QString &type = args.value( 6 );
    const QString &strategy = args.value( 7 );

    engine->addPosition( market, side, lo, hi, size, type, strategy );
}

void CommandRunner::command_setordermin( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    const qint32 &count = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_min = count;
    kDebug() << "market min for" << market << "is now" << count;
}

void CommandRunner::command_setordermax( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    const qint32 &count = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_max = count;
    kDebug() << "market max for" << market << "is now" << count;
}

void CommandRunner::command_setorderdc( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    const qint32 &count = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_dc = count;
    kDebug() << "market dc for" << market << "is now" << count;
}

void CommandRunner::command_setorderdcnice( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    const qint32 &nice = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_dc_nice = nice;
    kDebug() << "market dc nice for" << market << "is now" << nice;
}

void CommandRunner::command_setorderlandmarkthresh( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    const qint32 &val = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_landmark_thresh = val;
    kDebug() << "market landmark thresh for" << market << "is now" << val;
}

void CommandRunner::command_setorderlandmarkstart( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    const qint32 &val = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_landmark_start = val;
    kDebug() << "market landmark start for" << market << "is now" << val;
}

void CommandRunner::command_long( QStringList &args )
{
    engine->positions->flipLoSellPrice( args.value( 1 ), args.value( 2 ) );
}

void CommandRunner::command_longindex( QStringList &args )
{
    engine->positions->flipLoSellIndex( args.value( 1 ), args.value( 2 ) );
}

void CommandRunner::command_short( QStringList &args )
{
    engine->positions->flipHiBuyPrice( args.value( 1 ), args.value( 2 ) );
}

void CommandRunner::command_shortindex( QStringList &args )
{
    engine->positions->flipHiBuyIndex( args.value( 1 ), args.value( 2 ) );
}

void CommandRunner::command_setcancelthresh( QStringList &args )
{
    rest->market_cancel_thresh = args.value( 1 ).toInt();
    kDebug() << "cancel thresh changed to" << rest->market_cancel_thresh;
}

void CommandRunner::command_setkeyandsecret( QStringList &args )
{
    // [ exchange, key, secret ]
    if ( !checkArgs( args, 2 ) ) return;

    QByteArray key = args.value( 1 ).toLocal8Bit();
    QByteArray secret = args.value( 2 ).toLocal8Bit();

    rest->keystore.setKeys( key, secret );
    kDebug() << "key and secret set.";
}

void CommandRunner::command_getvolume( QStringList &args )
{
    Q_UNUSED( args )
    stats->printVolumes();
}

void CommandRunner::command_getdailyvolume( QStringList &args )
{
    Q_UNUSED( args )
    stats->printDailyVolumes();
}

void CommandRunner::command_getdailyprofit( QStringList &args )
{
    Q_UNUSED( args )
    stats->printDailyProfit();
}

void CommandRunner::command_getdailyfills( QStringList &args )
{
    Q_UNUSED( args )
    stats->printDailyFills();
}

void CommandRunner::command_getprofit( QStringList &args )
{
    Q_UNUSED( args )
    stats->printProfit();
}

void CommandRunner::command_getmarketprofit( QStringList &args )
{
    Q_UNUSED( args )
    stats->printMarketProfit();
}

void CommandRunner::command_getdailymarketprofit( QStringList &args )
{
    Q_UNUSED( args )
    stats->printDailyMarketProfit();
}

void CommandRunner::command_getdailymarketvolume( QStringList &args )
{
    Q_UNUSED( args )
    stats->printDailyMarketVolume();
}

void CommandRunner::command_getdailymarketprofitvolume( QStringList &args )
{
    Q_UNUSED( args )
    stats->printDailyMarketProfitVolume();
}

void CommandRunner::command_getmarketprofitvolume( QStringList &args )
{
    Q_UNUSED( args )
    stats->printMarketProfitVolume();
}

void CommandRunner::command_getmarketprofitfills( QStringList &args )
{
    Q_UNUSED( args )
    stats->printMarketProfitFills();
}

void CommandRunner::command_getdailymarketprofitriskreward( QStringList &args )
{
    Q_UNUSED( args )
    stats->printDailyMarketProfitRW();
}

void CommandRunner::command_getmarketprofitriskreward( QStringList &args )
{
    Q_UNUSED( args )
    stats->printMarketProfitRW();
}

void CommandRunner::command_getfills( QStringList &args )
{
    Q_UNUSED( args )
    stats->printFills();
}

void CommandRunner::command_getshortlong( QStringList &args )
{
    stats->printStrategyShortLong( args.value( 1 ) );
}

void CommandRunner::command_gethibuylosell( QStringList &args )
{
    Q_UNUSED( args )
    QMap<Coin, QString> market_spreads;

    for ( QHash<QString, MarketInfo>::const_iterator i = engine->getMarketInfoStructure().begin(); i != engine->getMarketInfoStructure().end(); i++ )
    {
        const Coin &hi_buy  = i.value().highest_buy;
        const Coin &lo_sell = i.value().lowest_sell;

        // calculate pct difference
        Coin pct_diff = ( Coin( 1 ) - ( hi_buy / lo_sell ) ) * 100;

        // format str
        QString pct_diff_str = pct_diff;
        pct_diff_str.truncate( 4 );
        pct_diff_str.append( QChar( '%' ) );

        QString out = QString( "%1 %2 %3  diff %4" )
                            .arg( i.key(), -11 )
                            .arg( hi_buy,  15 )
                            .arg( lo_sell, 15 )
                            .arg( pct_diff_str );

        market_spreads.insert( pct_diff, out );
    }

    // print spreads from smallest to largest
    for ( QMap<Coin, QString>::const_iterator i = market_spreads.begin(); i != market_spreads.end(); i++ )
        kDebug() << i.value();
}

void CommandRunner::command_setmarketsettings( QStringList &args )
{
    if ( !checkArgs( args, 9 ) ) return;

    const QString &market = args.value( 1 );

    engine->setMarketSettings( market,
                               args.value( 2 ).toInt(),
                               args.value( 3 ).toInt(),
                               args.value( 4 ).toInt(),
                               args.value( 5 ).toInt(),
                               args.value( 6 ).toInt(),
                               args.value( 7 ).toInt(),
                               args.value( 8 ).toInt() == 0 ? false : true,
                               args.value( 9 ).toDouble() );

    kDebug() << "loaded market settings for" << market;
}

void CommandRunner::command_setmarketoffset( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    qreal offset = args.value( 2 ).toDouble();

    engine->getMarketInfo( market ).market_offset = offset;
    kDebug() << "market offset for" << market << "is" << CoinAmount::toSatoshiFormat( offset );
}

void CommandRunner::command_setmarketsentiment( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );
    bool sentiment = args.value( 2 ) == "true" ? true : false;

    engine->getMarketInfo( market ).market_sentiment = sentiment;
    kDebug() << "market sentiment for" << market << "is" << QString( "%1" ).arg( sentiment ? "bullish" : "bearish" );
}

void CommandRunner::command_setnaminterval( QStringList &args )
{
    rest->send_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "nam interval set to" << rest->send_timer->interval();
}

void CommandRunner::command_setbookinterval( QStringList &args )
{
    rest->orderbook_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "bot orderbook interval set to" << rest->orderbook_timer->interval();
}

void CommandRunner::command_settickerinterval( QStringList &args )
{
    rest->ticker_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "ticker_timer interval set to" << rest->ticker_timer->interval();
}

void CommandRunner::command_setgracetimelimit( QStringList &args )
{
    engine->settings->stray_grace_time_limit = args.value( 1 ).toLong();
    kDebug() << "stray_grace_time_limit set to" << engine->settings->stray_grace_time_limit;
}

void CommandRunner::command_setcheckinterval( QStringList &args )
{
    rest->timeout_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "timeout check interval set to" << rest->timeout_timer->interval();
}

void CommandRunner::command_setdcinterval( QStringList &args )
{
    rest->diverge_converge_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "diverge converge interval set to" << rest->diverge_converge_timer->interval();
}

void CommandRunner::command_setclearstrayorders( QStringList &args )
{
    engine->settings->should_clear_stray_orders = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_clear_stray_orders set to" << engine->settings->should_clear_stray_orders;
}

void CommandRunner::command_setclearstrayordersall( QStringList &args )
{
    engine->settings->should_clear_stray_orders_all = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_clear_stray_orders_all set to" << engine->settings->should_clear_stray_orders_all;
}

void CommandRunner::command_setslippagecalculated( QStringList &args )
{
    engine->settings->should_slippage_be_calculated = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_slippage_be_calculated set to" << engine->settings->should_slippage_be_calculated;
}

void CommandRunner::command_setadjustbuysell( QStringList &args )
{
    engine->settings->should_adjust_hibuy_losell = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_adjust_hibuy_losell set to" << engine->settings->should_adjust_hibuy_losell;
}

void CommandRunner::command_setdcslippage( QStringList &args )
{
    engine->settings->should_dc_slippage_orders = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_dc_slippage_orders set to" << engine->settings->should_dc_slippage_orders;
}

void CommandRunner::command_setorderbookstaletolerance( QStringList &args )
{
    rest->orderbook_stale_tolerance = args.value( 1 ).toLongLong();
    kDebug() << "orderbook_stale_tolerance set to" << rest->orderbook_stale_tolerance << "ms";
}

void CommandRunner::command_setsafetydelaytime( QStringList &args )
{
    engine->settings->safety_delay_time = args.value( 1 ).toLongLong();
    kDebug() << "safety_delay_time set to" << engine->settings->safety_delay_time << "ms";
}

void CommandRunner::command_settickersafetydelaytime( QStringList &args )
{
    engine->settings->ticker_safety_delay_time = args.value( 1 ).toLongLong();
    kDebug() << "ticker_safety_delay_time set to" << engine->settings->ticker_safety_delay_time << "ms";
}

void CommandRunner::command_setslippagestaletime( QStringList &args )
{
    rest->slippage_stale_time = args.value( 1 ).toLongLong();
    kDebug() << "slippage_stale_time set to" << rest->slippage_stale_time << "ms";
}

void CommandRunner::command_setqueuedcommandsmax( QStringList &args )
{
    rest->limit_commands_queued = args.value( 1 ).toInt();
    kDebug() << "limit_commands_queued set to" << rest->limit_commands_queued;
}

void CommandRunner::command_setqueuedcommandsmaxdc( QStringList &args )
{
    rest->limit_commands_queued_dc_check = args.value( 1 ).toInt();
    kDebug() << "limit_commands_queued_dc_check set to" << rest->limit_commands_queued_dc_check;
}

void CommandRunner::command_setsentcommandsmax( QStringList &args )
{
    rest->limit_commands_sent = args.value( 1 ).toInt();
    kDebug() << "sent commands max set to" << rest->limit_commands_sent;
}

void CommandRunner::command_settimeoutyield( QStringList &args )
{
    rest->limit_timeout_yield = args.value( 1 ).toInt();
    kDebug() << "limit_timeout_yield set to" << rest->limit_timeout_yield;
}

void CommandRunner::command_setrequesttimeout( QStringList &args )
{
    engine->settings->request_timeout = args.value( 1 ).toLong();
    kDebug() << "request timeout is" << engine->settings->request_timeout;
}

void CommandRunner::command_setcanceltimeout( QStringList &args )
{
    engine->settings->cancel_timeout = args.value( 1 ).toLong();
    kDebug() << "cancel timeout is" << engine->settings->cancel_timeout;
}

void CommandRunner::command_setslippagetimeout( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    const QString &market = args.value( 1 );

    engine->getMarketInfo( market ).slippage_timeout = args.value( 2 ).toInt();
    kDebug() << "slippage timeout for" << market << "is" << engine->getMarketInfo( market ).slippage_timeout;
}

void CommandRunner::command_setsprucebasecurrency( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->spruce.setBaseCurrency( args.value( 1 ) );
    kDebug() << "spruce base currency is now" << engine->spruce.getBaseCurrency();
}

void CommandRunner::command_setspruceweight( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    engine->spruce.setMarketWeight( args.value( 1 ),
                                    args.value( 2 ) );
    kDebug() << "spruce market weight for" << args.value( 1 ) << "is" << args.value( 2 );
}

void CommandRunner::command_setsprucestartnode( QStringList &args )
{
    if ( !checkArgs( args, 3 ) ) return;

    engine->spruce.addStartNode( args.value( 1 ),
                                 args.value( 2 ),
                                 args.value( 3 ) );
    kDebug() << "spruce added start node for" << args.value( 1 ) << args.value( 2 ) << args.value( 3 );
}

void CommandRunner::command_setspruceshortlongtotal( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    engine->spruce.addToShortLonged( args.value( 1 ),
                                     args.value( 2 ) );
    kDebug() << "spruce shortlong total for" << args.value( 1 ) << "is" << args.value( 2 );
}

void CommandRunner::command_getconfig( QStringList &args )
{
    const QString &market = args.value( 1 );

    // print all market options
    if ( market.isEmpty() || market == ALL )
    {
        for ( QHash<QString, MarketInfo>::iterator i = engine->getMarketInfoStructure().begin(); i != engine->getMarketInfoStructure().end(); i++ )
        {
            kDebug() << QString( "%1 %2" )
                        .arg( i.key(), -10 )
                        .arg( i.value() );
        }
    }
    else // print only one market, and return (we supplied a market, don't print global options)
    {
        kDebug() << engine->getMarketInfoStructure().value( market );
        return;
    }

    //kDebug() << "market info      " << engine->getMarketInfoStructure();

#if defined(EXCHANGE_POLONIEX)
    kDebug() << "slippage_multipli" << rest->slippage_multiplier;
#endif
    kDebug() << "limit_commands_queued =" << rest->limit_commands_queued;
    kDebug() << "limit_commands_queued_dc_check =" << rest->limit_commands_queued_dc_check;
    kDebug() << "limit_commands_sent =" << rest->limit_commands_sent;
    kDebug() << "limit_timeout_yield =" << rest->limit_timeout_yield;
    kDebug() << "market_cancel_thresh =" << rest->market_cancel_thresh;
    kDebug() << "request_timeout =" << engine->settings->request_timeout;
    kDebug() << "cancel_timeout =" << engine->settings->cancel_timeout;
    kDebug() << "should_clear_stray_orders =" << engine->settings->should_clear_stray_orders;
    kDebug() << "should_clear_stray_orders_all =" << engine->settings->should_clear_stray_orders_all;
    kDebug() << "should_slippage_be_calculated =" << engine->settings->should_slippage_be_calculated;
    kDebug() << "should_adjust_hibuy_losell =" << engine->settings->should_adjust_hibuy_losell;
    kDebug() << "should_adjust_hibuy_losell_debugmsgs_ticker =" << engine->settings->should_adjust_hibuy_losell_debugmsgs_ticker;
    kDebug() << "should_mitigate_blank_orderbook_flash =" << engine->settings->should_mitigate_blank_orderbook_flash;
    kDebug() << "should_dc_slippage_orders =" << engine->settings->should_dc_slippage_orders;
    kDebug() << "stray_grace_time_limit =" << engine->settings->stray_grace_time_limit;
    kDebug() << "safety_delay_time =" << engine->settings->safety_delay_time;
    kDebug() << "ticker_safety_delay_time =" << engine->settings->ticker_safety_delay_time;
    kDebug() << "slippage_stale_time =" << rest->slippage_stale_time;
    kDebug() << "orderbook_stale_tolerance =" << rest->orderbook_stale_tolerance;

    kDebug() << "nam interval =" << rest->send_timer->interval();
    kDebug() << "orderbook update interval =" << rest->orderbook_timer->interval();

#if defined(EXCHANGE_BITTREX)
    kDebug() << "order_history_timer interval =" << rest->order_history_timer->interval();
#endif

    kDebug() << "ticker interval =" << rest->ticker_timer->interval();
    kDebug() << "timeout interval =" << rest->timeout_timer->interval();
    kDebug() << "dc interval =" << rest->diverge_converge_timer->interval();
    kDebug() << "is_chatty = " << engine->settings->is_chatty;
}

void CommandRunner::command_getinternal( QStringList &args )
{
    Q_UNUSED( args )
    engine->printInternal();

    kDebug() << "nam_queue size:" << rest->nam_queue.size();
    kDebug() << "nam_queue_sent size:" << rest->nam_queue_sent.size();
    kDebug() << "orderbook_update_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_update_time ).toString();
    kDebug() << "orderbook_update_request_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_update_request_time ).toString();
    kDebug() << "orderbook_public_update_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_public_update_time ).toString();
    kDebug() << "orderbook_public_update_request_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_public_update_request_time ).toString();


    kDebug() << "avg response time:" << rest->avg_response_time.avgResponseTime();
    kDebug() << "wss_heartbeat_time:" << QDateTime::fromMSecsSinceEpoch( rest->wss_heartbeat_time ).toString();
    kDebug() << "wss_connect_try_time:" << QDateTime::fromMSecsSinceEpoch( rest->wss_connect_try_time ).toString();
    kDebug() << "orders_stale_trip_count: " << rest->orders_stale_trip_count;
    kDebug() << "books_stale_trip_count: " << rest->books_stale_trip_count;
    kDebug() << "nonce:" << rest->request_nonce;
#if defined(EXCHANGE_BINANCE)
    kDebug() << "ratelimit_second:" << rest->ratelimit_second;
    kDebug() << "ratelimit_minute:" << rest->ratelimit_minute;
    kDebug() << "ratelimit_day:" << rest->ratelimit_day;
#elif defined(EXCHANGE_BITTREX)
    kDebug() << "order_history_update_time:" << QDateTime::fromMSecsSinceEpoch( rest->order_history_update_time ).toString();
#endif

    kDebug() << Global::getBuildString();
}

void CommandRunner::command_setmaintenancetime( QStringList &args )
{
    qint64 time = args.value( 1 ).toLongLong();

    // make sure time is valid. we shouldn't be able to set it to a sooner time unless it's 0.
    if ( time <= QDateTime::currentMSecsSinceEpoch() && time > 0 )
    {
        kDebug() << "local warning: avoided setting maintenance time because it was stale!";
        return;
    }

    engine->setMaintenanceTime( time );
    kDebug() << "maintenance status has been reset and maintenance_time set to" << engine->getMaintenanceTime();
}

void CommandRunner::command_clearstratstats( QStringList &args )
{
    stats->clearSome( args.value( 1 ) );
}

void CommandRunner::command_clearallstats( QStringList &args )
{
    Q_UNUSED( args )
    stats->clearAll();
}

void CommandRunner::command_savemarket( QStringList &args )
{
    engine->saveMarket( args.value( 1 ), args.value( 2 ).toInt() );
}

void CommandRunner::command_savesettings( QStringList &args )
{
    Q_UNUSED( args )
    engine->saveSettings();
}

void CommandRunner::command_sendcommand( QStringList &args )
{
    rest->sendRequest( args.value( 1 ), args.value( 2 ) );
}

void CommandRunner::command_setchatty( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    bool chatty = args.value( 1 ) == "true" ? true : false;

    engine->settings->is_chatty = chatty;
    kDebug() << "is_chatty set to" << chatty;
}

void CommandRunner::command_exit( QStringList &args )
{
    Q_UNUSED( args )
    emit exitSignal();
}

#if defined(EXCHANGE_BITTREX)
void CommandRunner::command_sethistoryinterval( QStringList &args )
{
    rest->order_history_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "bot order_history_timer interval set to" << rest->order_history_timer->interval();
}
#endif

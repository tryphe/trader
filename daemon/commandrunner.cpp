#include "commandrunner.h"
#include "global.h"
#include "engine.h"
#include "enginesettings.h"
#include "trexrest.h"
#include "polorest.h"
#include "bncrest.h"
#include "stats.h"
#include "positionman.h"
#include "market.h"
#include "alphatracker.h"
#include "spruce.h"
#include "spruceoverseer.h"

#include <functional>
#include <QString>
#include <QMap>
#include <QQueue>
#include <QTimer>

CommandRunner::CommandRunner(const quint8 _engine_type, Engine *_e, void *_rest, QObject *parent )
    : QObject( parent ),
      engine( _e )
{
    engine_type = _engine_type;

    if ( engine_type == ENGINE_BITTREX )
        rest_trex = static_cast<TrexREST*>( _rest );
    else if ( engine_type == ENGINE_BINANCE )
        rest_bnc = static_cast<BncREST*>( _rest );
    else if ( engine_type == ENGINE_POLONIEX )
        rest_polo = static_cast<PoloREST*>( _rest );

    // map strings to functions
    using std::placeholders::_1;
    command_map.insert( "getbalances", std::bind( &CommandRunner::command_getbalances, this, _1 ) );
    command_map.insert( "getlastprices", std::bind( &CommandRunner::command_getlastprices, this, _1 ) );
    command_map.insert( "getbuyselltotal", std::bind( &CommandRunner::command_getbuyselltotal, this, _1 ) );
    command_map.insert( "cancelall", std::bind( &CommandRunner::command_cancelall, this, _1 ) );
    command_map.insert( "cancellocal", std::bind( &CommandRunner::command_cancellocal, this, _1 ) );
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
    command_map.insert( "getdailyvolume", std::bind( &CommandRunner::command_getdailyvolume, this, _1 ) );
    command_map.insert( "getdailyfills", std::bind( &CommandRunner::command_getdailyfills, this, _1 ) );
    command_map.insert( "getalpha", std::bind( &CommandRunner::command_getalpha, this, _1 ) );
    command_map.insert( "getdailymarketvolume", std::bind( &CommandRunner::command_getdailymarketvolume, this, _1 ) );
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
    command_map.insert( "setspruceleverage", std::bind( &CommandRunner::command_setspruceleverage, this, _1 ) );
    command_map.insert( "setspruceprofile", std::bind( &CommandRunner::command_setspruceprofile, this, _1 ) );
    command_map.insert( "setsprucereserve", std::bind( &CommandRunner::command_setsprucereserve, this, _1 ) );
    command_map.insert( "setspruceordergreed", std::bind( &CommandRunner::command_setspruceordergreed, this, _1 ) );
    command_map.insert( "setsprucelongmax", std::bind( &CommandRunner::command_setsprucelongmax, this, _1 ) );
    command_map.insert( "setspruceshortmax", std::bind( &CommandRunner::command_setspruceshortmax, this, _1 ) );
    command_map.insert( "setsprucemarketmax", std::bind( &CommandRunner::command_setsprucemarketmax, this, _1 ) );
    command_map.insert( "setspruceordersize", std::bind( &CommandRunner::command_setspruceordersize, this, _1 ) );
    command_map.insert( "setspruceordernice", std::bind( &CommandRunner::command_setspruceordernice, this, _1 ) );
    command_map.insert( "setspruceallocation", std::bind( &CommandRunner::command_setspruceallocation, this, _1 ) );
    command_map.insert( "getconfig", std::bind( &CommandRunner::command_getconfig, this, _1 ) );
    command_map.insert( "getinternal", std::bind( &CommandRunner::command_getinternal, this, _1 ) );
    command_map.insert( "setmaintenancetime", std::bind( &CommandRunner::command_setmaintenancetime, this, _1 ) );
    command_map.insert( "clearallstats", std::bind( &CommandRunner::command_clearallstats, this, _1 ) );
    command_map.insert( "savemarket", std::bind( &CommandRunner::command_savemarket, this, _1 ) );
    command_map.insert( "savesettings", std::bind( &CommandRunner::command_savesettings, this, _1 ) );
    command_map.insert( "savestats", std::bind( &CommandRunner::command_savestats, this, _1 ) );
    command_map.insert( "sendcommand", std::bind( &CommandRunner::command_sendcommand, this, _1 ) );
    command_map.insert( "setchatty", std::bind( &CommandRunner::command_setchatty, this, _1 ) );
    command_map.insert( "spruceup", std::bind( &CommandRunner::command_spruceup, this, _1 ) );
    command_map.insert( "exit", std::bind( &CommandRunner::command_exit, this, _1 ) );
    command_map.insert( "stop", std::bind( &CommandRunner::command_exit, this, _1 ) );
    command_map.insert( "quit", std::bind( &CommandRunner::command_exit, this, _1 ) );

    kDebug() << QString( "[CommandRunner %1]" )
                .arg( engine_type );
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

    const QString prefix = QString( "[CommandRunner %1]" )
                            .arg( engine_type );

    // parse all commands
    while ( !commands.isEmpty() )
    {
        QStringList args = commands.takeFirst();
        QString cmd = args.first();

        qint32 times = times_called.value( cmd );

        // if the command is unknown, print it until suppressed
        if ( !command_map.contains( cmd ) )
        {
            kDebug() << prefix << "unknown command:" << cmd;
            continue;
        }

        // do not leak key/secret into debug log
        if ( cmd.startsWith( "setkey" ) )
        {
            kDebug() << prefix << "running setkey...";
        }
        // be nice to the log
        else if ( times > 10 )
        {
            times_called[ cmd ] = 0; // don't print any more lines for this command
            kDebug() << QString( "%1 running '%2' x%3" )
                            .arg( prefix )
                            .arg( cmd )
                            .arg( times );
        }
        else if ( times > 0 )
        {
            kDebug() << prefix << "running:" << args.join( QChar( ' ' ) );
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
                        .arg( engine->getPositionMan()->getMarketOrderTotal( market ) )
                        .arg( engine->getPositionMan()->getMarketOrderTotal( market, true ) );
    }
}

bool CommandRunner::checkArgs( const QStringList &args, qint32 expected_args_min, qint32 expected_args_max )
{
    expected_args_min++; // add one
    if ( expected_args_max < 0 )
        expected_args_max = expected_args_min;

    const QString prefix = QString( "[CommandRunner %1]" )
                            .arg( engine_type );

    // check for expected arg count
    if ( args.size() < expected_args_min || args.size() > expected_args_max )
    {
        kDebug() << QString( "%1 not enough args (%2)" )
                        .arg( prefix )
                        .arg( expected_args_min );
        return false;
    }

    return true;
}

void CommandRunner::command_getbalances( QStringList & )
{
    if ( engine_type == ENGINE_BITTREX )
        rest_trex->sendRequest( TREX_COMMAND_GET_BALANCES );
    else if ( engine_type == ENGINE_BINANCE )
        rest_bnc->sendRequest( BNC_COMMAND_GETBALANCES, "", nullptr, 5 );
    else if ( engine_type == ENGINE_POLONIEX )
        rest_polo->sendRequest( POLO_COMMAND_GETBALANCES );
}

void CommandRunner::command_getlastprices( QStringList & )
{
    //stats->printLastPrices();
}

void CommandRunner::command_getbuyselltotal( QStringList & )
{
    QMap<QString /*market*/, qint32> buys, sells, total;
    qint32 total_overall = 0;

    // build indexes from active and queued positions
    QSet<Position*>::const_iterator begin = engine->getPositionMan()->all().begin(),
                                    end = engine->getPositionMan()->all().end();
    for ( QSet<Position*>::const_iterator i = begin; i != end; i++ )
    {
        Position *const &pos = *i;

        if ( pos->side == SIDE_BUY )
            buys[ pos->market ]++;
        else
            sells[ pos->market ]++;

        // save the total count
        total[ pos->market ]++;
        total_overall++;
    }

    // spacing:           10   5          5          5
    kDebug() << QString( "%1 | buys  | sells | total" )
                .arg( QString(), MARKET_STRING_WIDTH ); // padding

    for ( QMap<QString, qint32>::const_iterator i = total.begin(); i != total.end(); i++ )
    {
        const QString &market = i.key();

        kDebug() << QString( "%1 | >>>grn<<<%2>>>none<<< | >>>red<<<%3>>>none<<< | %4" )
                    .arg( market, -MARKET_STRING_WIDTH )
                    .arg( buys.value( market ), -5 )
                    .arg( sells.value( market ), -5 )
                    .arg( total.value( market ), -5 );
    }

    kDebug() << QString( "%1 | %2 | %3 | %4" )
                .arg( QString(), -MARKET_STRING_WIDTH )
                .arg( QString(), -5 )
                .arg( QString(), -5 )
                .arg( total_overall, -5 );
}

void CommandRunner::command_cancelall( QStringList &args )
{
    engine->getPositionMan()->cancelAll( Market( args.value( 1 ) ) );
}

void CommandRunner::command_cancellocal( QStringList &args )
{
    engine->getPositionMan()->cancelLocal( Market( args.value( 1 ) ) );
}

void CommandRunner::command_cancelhighest( QStringList &args )
{
    engine->getPositionMan()->cancelHighest( Market( args.value( 1 ) ) );
}

void CommandRunner::command_cancellowest( QStringList &args )
{
    engine->getPositionMan()->cancelLowest( Market( args.value( 1 ) ) );
}

void CommandRunner::command_getorders( QStringList &args )
{
    Q_UNUSED( args )
    //stats->printOrders( Market( args.value( 1 ) ), false );
}

void CommandRunner::command_getpositions( QStringList &args )
{
    Q_UNUSED( args )
    //stats->printPositions( Market( args.value( 1 ) ) );
}

void CommandRunner::command_getordersbyindex( QStringList &args )
{
    Q_UNUSED( args )
    //stats->printOrders( Market( args.value( 1 ) ), true );
}

void CommandRunner::command_setorder( QStringList &args )
{
    if ( !checkArgs( args, 6, 7 ) ) return;

    QString market = Market( args.value( 1 ) );
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

    QString market = Market( args.value( 1 ) );
    const qint32 &count = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_min = count;
    kDebug() << "market min for" << market << "is now" << count;
}

void CommandRunner::command_setordermax( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    QString market = Market( args.value( 1 ) );
    const qint32 &count = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_max = count;
    kDebug() << "market max for" << market << "is now" << count;
}

void CommandRunner::command_setorderdc( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    QString market = Market( args.value( 1 ) );
    const qint32 &count = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_dc = count;
    kDebug() << "market dc for" << market << "is now" << count;
}

void CommandRunner::command_setorderdcnice( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    QString market = Market( args.value( 1 ) );
    const qint32 &nice = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_dc_nice = nice;
    kDebug() << "market dc nice for" << market << "is now" << nice;
}

void CommandRunner::command_setorderlandmarkthresh( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    QString market = Market( args.value( 1 ) );
    const qint32 &val = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_landmark_thresh = val;
    kDebug() << "market landmark thresh for" << market << "is now" << val;
}

void CommandRunner::command_setorderlandmarkstart( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    QString market = Market( args.value( 1 ) );
    const qint32 &val = args.value( 2 ).toInt();

    engine->getMarketInfo( market ).order_landmark_start = val;
    kDebug() << "market landmark start for" << market << "is now" << val;
}

void CommandRunner::command_long( QStringList &args )
{
    QString market = Market( args.value( 1 ) );
    engine->getPositionMan()->flipLoSellPrice( market, args.value( 2 ) );
}

void CommandRunner::command_longindex( QStringList &args )
{
    QString market = Market( args.value( 1 ) );
    engine->getPositionMan()->flipLoSellIndex( market, args.value( 2 ) );
}

void CommandRunner::command_short( QStringList &args )
{
    QString market = Market( args.value( 1 ) );
    engine->getPositionMan()->flipHiBuyPrice( market, args.value( 2 ) );
}

void CommandRunner::command_shortindex( QStringList &args )
{
    QString market = Market( args.value( 1 ) );
    engine->getPositionMan()->flipHiBuyIndex( market, args.value( 2 ) );
}

void CommandRunner::command_setcancelthresh( QStringList &args )
{
    qint32 &market_cancel_thresh = rest_trex->market_cancel_thresh;

    if ( engine_type == ENGINE_BINANCE )
        market_cancel_thresh = rest_bnc->market_cancel_thresh;
    else if ( engine_type == ENGINE_POLONIEX )
        market_cancel_thresh = rest_polo->market_cancel_thresh;

    market_cancel_thresh = args.value( 1 ).toInt();

    kDebug() << "cancel thresh changed to" << market_cancel_thresh;
}

void CommandRunner::command_setkeyandsecret( QStringList &args )
{
    Q_UNUSED( args )
    // [ exchange, key, secret ]
//    if ( !checkArgs( args, 2 ) ) return;

//    QByteArray key = args.value( 1 ).toLocal8Bit();
//    QByteArray secret = args.value( 2 ).toLocal8Bit();

//    rest->keystore.setKeys( key, secret );
    kDebug() << "local error: please hardcode your keys.";
}

void CommandRunner::command_getdailyvolume( QStringList &args )
{
    Q_UNUSED( args )
    engine->alpha->printDailyVolume();
}

void CommandRunner::command_getdailyfills( QStringList &args )
{
    Q_UNUSED( args )
    //stats->printDailyFills();
}

void CommandRunner::command_getalpha( QStringList &args )
{
    Q_UNUSED( args )
    engine->alpha->printAlpha();
}

void CommandRunner::command_getdailymarketvolume( QStringList &args )
{
    Q_UNUSED( args )
    //stats->printDailyMarketVolume();
}

void CommandRunner::command_getshortlong( QStringList &args )
{
    Q_UNUSED( args )
    //stats->printStrategyShortLong( Market( args.value( 1 ) ) );
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

    QString market = Market( args.value( 1 ) );

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

    QString market = Market( args.value( 1 ) );
    qreal offset = args.value( 2 ).toDouble();

    engine->getMarketInfo( market ).market_offset = offset;
    kDebug() << "market offset for" << market << "is" << CoinAmount::toSatoshiFormat( offset );
}

void CommandRunner::command_setmarketsentiment( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    QString market = Market( args.value( 1 ) );
    bool sentiment = args.value( 2 ) == "true" ? true : false;

    engine->getMarketInfo( market ).market_sentiment = sentiment;
    kDebug() << "market sentiment for" << market << "is" << QString( "%1" ).arg( sentiment ? "bullish" : "bearish" );
}

void CommandRunner::command_setnaminterval( QStringList &args )
{
    int new_interval = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->send_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_trex->send_timer->interval();
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->send_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_bnc->send_timer->interval();
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->send_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_polo->send_timer->interval();
    }

    kDebug() << "nam interval set to" << new_interval;
}

void CommandRunner::command_setbookinterval( QStringList &args )
{
    int new_interval = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->orderbook_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_trex->orderbook_timer->interval();
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->orderbook_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_bnc->orderbook_timer->interval();
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->orderbook_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_polo->orderbook_timer->interval();
    }

    kDebug() << "bot orderbook interval set to" << new_interval;
}

void CommandRunner::command_settickerinterval( QStringList &args )
{
    int new_interval = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->ticker_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_trex->ticker_timer->interval();
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->ticker_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_bnc->ticker_timer->interval();
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->ticker_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_polo->ticker_timer->interval();
    }

    kDebug() << "ticker_timer interval set to" << new_interval;
}

void CommandRunner::command_setgracetimelimit( QStringList &args )
{
    engine->getSettings()->stray_grace_time_limit = args.value( 1 ).toLong();
    kDebug() << "stray_grace_time_limit set to" << engine->getSettings()->stray_grace_time_limit;
}

void CommandRunner::command_setcheckinterval( QStringList &args )
{
    int new_interval = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->timeout_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_trex->timeout_timer->interval();
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->timeout_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_bnc->timeout_timer->interval();
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->timeout_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_polo->timeout_timer->interval();
    }

    kDebug() << "timeout check interval set to" << new_interval;
}

void CommandRunner::command_setdcinterval( QStringList &args )
{
    int new_interval = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->diverge_converge_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_trex->diverge_converge_timer->interval();
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->diverge_converge_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_bnc->diverge_converge_timer->interval();
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->diverge_converge_timer->setInterval( args.value( 1 ).toInt() );
        new_interval = rest_polo->diverge_converge_timer->interval();
    }

    kDebug() << "diverge converge interval set to" << new_interval;
}

void CommandRunner::command_setclearstrayorders( QStringList &args )
{
    engine->getSettings()->should_clear_stray_orders = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_clear_stray_orders set to" << engine->getSettings()->should_clear_stray_orders;
}

void CommandRunner::command_setclearstrayordersall( QStringList &args )
{
    engine->getSettings()->should_clear_stray_orders_all = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_clear_stray_orders_all set to" << engine->getSettings()->should_clear_stray_orders_all;
}

void CommandRunner::command_setslippagecalculated( QStringList &args )
{
    engine->getSettings()->should_slippage_be_calculated = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_slippage_be_calculated set to" << engine->getSettings()->should_slippage_be_calculated;
}

void CommandRunner::command_setadjustbuysell( QStringList &args )
{
    engine->getSettings()->should_adjust_hibuy_losell = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_adjust_hibuy_losell set to" << engine->getSettings()->should_adjust_hibuy_losell;
}

void CommandRunner::command_setdcslippage( QStringList &args )
{
    engine->getSettings()->should_dc_slippage_orders = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_dc_slippage_orders set to" << engine->getSettings()->should_dc_slippage_orders;
}

void CommandRunner::command_setorderbookstaletolerance( QStringList &args )
{
    qlonglong new_tolerance = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->orderbook_stale_tolerance = args.value( 1 ).toLongLong();
        new_tolerance = rest_trex->orderbook_stale_tolerance;
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->orderbook_stale_tolerance = args.value( 1 ).toLongLong();
        new_tolerance = rest_bnc->orderbook_stale_tolerance;
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->orderbook_stale_tolerance = args.value( 1 ).toLongLong();
        new_tolerance = rest_polo->orderbook_stale_tolerance;
    }

    kDebug() << "orderbook_stale_tolerance set to" << new_tolerance << "ms";
}

void CommandRunner::command_setsafetydelaytime( QStringList &args )
{
    engine->getSettings()->safety_delay_time = args.value( 1 ).toLongLong();
    kDebug() << "safety_delay_time set to" << engine->getSettings()->safety_delay_time << "ms";
}

void CommandRunner::command_settickersafetydelaytime( QStringList &args )
{
    engine->getSettings()->ticker_safety_delay_time = args.value( 1 ).toLongLong();
    kDebug() << "ticker_safety_delay_time set to" << engine->getSettings()->ticker_safety_delay_time << "ms";
}

void CommandRunner::command_setslippagestaletime( QStringList &args )
{
    qlonglong new_tolerance = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->slippage_stale_time = args.value( 1 ).toLongLong();
        new_tolerance = rest_trex->slippage_stale_time;
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->slippage_stale_time = args.value( 1 ).toLongLong();
        new_tolerance = rest_bnc->slippage_stale_time;
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->slippage_stale_time = args.value( 1 ).toLongLong();
        new_tolerance = rest_polo->slippage_stale_time;
    }

    kDebug() << "slippage_stale_time set to" << new_tolerance << "ms";
}

void CommandRunner::command_setqueuedcommandsmax( QStringList &args )
{
    int new_limit = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->limit_commands_queued = args.value( 1 ).toInt();
        new_limit = rest_trex->limit_commands_queued;
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->limit_commands_queued = args.value( 1 ).toInt();
        new_limit = rest_bnc->limit_commands_queued;
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->limit_commands_queued = args.value( 1 ).toInt();
        new_limit = rest_polo->limit_commands_queued;
    }

    kDebug() << "limit_commands_queued set to" << new_limit;
}

void CommandRunner::command_setqueuedcommandsmaxdc( QStringList &args )
{
    int new_limit = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->limit_commands_queued_dc_check = args.value( 1 ).toInt();
        new_limit = rest_trex->limit_commands_queued_dc_check;
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->limit_commands_queued_dc_check = args.value( 1 ).toInt();
        new_limit = rest_bnc->limit_commands_queued_dc_check;
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->limit_commands_queued_dc_check = args.value( 1 ).toInt();
        new_limit = rest_polo->limit_commands_queued_dc_check;
    }

    kDebug() << "limit_commands_queued_dc_check set to" << new_limit;
}

void CommandRunner::command_setsentcommandsmax( QStringList &args )
{
    int new_limit = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->limit_commands_sent = args.value( 1 ).toInt();
        new_limit = rest_trex->limit_commands_sent;
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->limit_commands_sent = args.value( 1 ).toInt();
        new_limit = rest_bnc->limit_commands_sent;
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->limit_commands_sent = args.value( 1 ).toInt();
        new_limit = rest_polo->limit_commands_sent;
    }

    kDebug() << "sent commands max set to" << new_limit;
}

void CommandRunner::command_settimeoutyield( QStringList &args )
{
    int new_limit = 0;

    if ( engine_type == ENGINE_BITTREX )
    {
        rest_trex->limit_timeout_yield = args.value( 1 ).toInt();
        new_limit = rest_trex->limit_timeout_yield;
    }
    else if ( engine_type == ENGINE_BINANCE )
    {
        rest_bnc->limit_timeout_yield = args.value( 1 ).toInt();
        new_limit = rest_bnc->limit_timeout_yield;
    }
    else if ( engine_type == ENGINE_POLONIEX )
    {
        rest_polo->limit_timeout_yield = args.value( 1 ).toInt();
        new_limit = rest_polo->limit_timeout_yield;
    }

    kDebug() << "limit_timeout_yield set to" << new_limit;
}

void CommandRunner::command_setrequesttimeout( QStringList &args )
{
    engine->getSettings()->request_timeout = args.value( 1 ).toLong();
    kDebug() << "request timeout is" << engine->getSettings()->request_timeout;
}

void CommandRunner::command_setcanceltimeout( QStringList &args )
{
    engine->getSettings()->cancel_timeout = args.value( 1 ).toLong();
    kDebug() << "cancel timeout is" << engine->getSettings()->cancel_timeout;
}

void CommandRunner::command_setslippagetimeout( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    QString market = Market( args.value( 1 ) );

    engine->getMarketInfo( market ).slippage_timeout = args.value( 2 ).toInt();
    kDebug() << "slippage timeout for" << market << "is" << engine->getMarketInfo( market ).slippage_timeout;
}

void CommandRunner::command_setsprucebasecurrency( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    spruce_overseer->spruce->setBaseCurrency( args.value( 1 ) );
    kDebug() << "spruce base currency is now" << spruce_overseer->spruce->getBaseCurrency();
}

void CommandRunner::command_setspruceweight( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    spruce_overseer->spruce->setCurrencyWeight( args.value( 1 ), args.value( 2 ) );
    kDebug() << "spruce currency weight for" << args.value( 1 ) << "is" << args.value( 2 );
}

void CommandRunner::command_setsprucestartnode( QStringList &args )
{
    if ( !checkArgs( args, 3 ) ) return;

    spruce_overseer->spruce->addStartNode( args.value( 1 ), args.value( 2 ), args.value( 3 ) );
    kDebug() << "spruce added start node for" << args.value( 1 ) << args.value( 2 ) << args.value( 3 );
}

void CommandRunner::command_setspruceshortlongtotal( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    spruce_overseer->spruce->addToShortLonged( Market( args.value( 1 ) ), args.value( 2 ) );
    kDebug() << "spruce shortlong total for" << args.value( 1 ) << "is" << args.value( 2 );
}

void CommandRunner::command_setspruceleverage( QStringList &args )
{
    spruce_overseer->spruce->setLeverage( args.value( 1 ) );
    kDebug() << "spruce log leverage is" << spruce_overseer->spruce->getLeverage();
}

void CommandRunner::command_setspruceprofile( QStringList &args )
{
    spruce_overseer->spruce->setProfileU( args.value( 1 ), args.value( 2 ) );
    kDebug() << "spruce profile u for" << args.value( 1 ) << "is" << spruce_overseer->spruce->getProfileU( args.value( 1 ) );
}

void CommandRunner::command_setsprucereserve( QStringList &args )
{
    spruce_overseer->spruce->setReserve( args.value( 1 ), args.value( 2 ) );
    kDebug() << "spruce reserve for" << args.value( 1 ) << "is" << spruce_overseer->spruce->getReserve( args.value( 1 ) );
}

void CommandRunner::command_setspruceordergreed( QStringList &args )
{
    spruce_overseer->spruce->setOrderGreed( args.value( 1 ) );
    spruce_overseer->spruce->setOrderRandomBuy( args.value( 2 ) );
    spruce_overseer->spruce->setOrderRandomSell( args.value( 3 ) );
    kDebug() << "spruce order greed is" << args.value( 1 ) << args.value( 2 ) << args.value( 3 );
}

void CommandRunner::command_setsprucelongmax( QStringList &args )
{
    spruce_overseer->spruce->setLongMax( args.value( 1 ) );
    kDebug() << "spruce longmax is" << spruce_overseer->spruce->getLongMax();
}

void CommandRunner::command_setspruceshortmax( QStringList &args )
{
    spruce_overseer->spruce->setShortMax( args.value( 1 ) );
    kDebug() << "spruce shortmax is" << spruce_overseer->spruce->getShortMax();
}

void CommandRunner::command_setsprucemarketmax( QStringList &args )
{
    spruce_overseer->spruce->setMarketBuyMax( args.value( 1 ) );
    spruce_overseer->spruce->setMarketSellMax( args.value( 2 ) );
    kDebug() << "spruce marketmax is" << spruce_overseer->spruce->getMarketBuyMax()
                                      << spruce_overseer->spruce->getMarketSellMax();
}

void CommandRunner::command_setspruceordersize( QStringList &args )
{
    spruce_overseer->spruce->setOrderSize( args.value( 1 ) );
    kDebug() << "spruce ordersize is" << spruce_overseer->spruce->getOrderSize();
}

void CommandRunner::command_setspruceordernice( QStringList &args )
{
    spruce_overseer->spruce->setOrderNice( args.value( 1 ) );
    spruce_overseer->spruce->setOrderNiceSpreadPut( args.value( 2 ) );
    spruce_overseer->spruce->setOrderNiceZeroBound( args.value( 3 ) );
    kDebug() << "spruce order nice is" << spruce_overseer->spruce->getOrderNice()
                                       << spruce_overseer->spruce->getOrderNiceSpreadPut()
                                       << spruce_overseer->spruce->getOrderNiceZeroBound();
}

void CommandRunner::command_setspruceallocation( QStringList &args )
{
    spruce_overseer->spruce->setExchangeAllocation( args.value( 1 ),
                                                    Coin( args.value( 2 ) ) );
}

void CommandRunner::command_spruceup( QStringList & )
{
    spruce_overseer->onSpruceUp();
}

void CommandRunner::command_getconfig( QStringList &args )
{
    QString market = Market( args.value( 1 ) );

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

//    kDebug() << "limit_commands_queued =" << rest->limit_commands_queued;
//    kDebug() << "limit_commands_queued_dc_check =" << rest->limit_commands_queued_dc_check;
//    kDebug() << "limit_commands_sent =" << rest->limit_commands_sent;
//    kDebug() << "limit_timeout_yield =" << rest->limit_timeout_yield;
//    kDebug() << "market_cancel_thresh =" << rest->market_cancel_thresh;
//    kDebug() << "request_timeout =" << engine->getSettings()->request_timeout;
//    kDebug() << "cancel_timeout =" << engine->getSettings()->cancel_timeout;
//    kDebug() << "should_clear_stray_orders =" << engine->getSettings()->should_clear_stray_orders;
//    kDebug() << "should_clear_stray_orders_all =" << engine->getSettings()->should_clear_stray_orders_all;
//    kDebug() << "should_slippage_be_calculated =" << engine->getSettings()->should_slippage_be_calculated;
//    kDebug() << "should_adjust_hibuy_losell =" << engine->getSettings()->should_adjust_hibuy_losell;
//    kDebug() << "should_adjust_hibuy_losell_debugmsgs_ticker =" << engine->getSettings()->should_adjust_hibuy_losell_debugmsgs_ticker;
//    kDebug() << "should_mitigate_blank_orderbook_flash =" << engine->getSettings()->should_mitigate_blank_orderbook_flash;
//    kDebug() << "should_dc_slippage_orders =" << engine->getSettings()->should_dc_slippage_orders;
//    kDebug() << "stray_grace_time_limit =" << engine->getSettings()->stray_grace_time_limit;
//    kDebug() << "safety_delay_time =" << engine->getSettings()->safety_delay_time;
//    kDebug() << "ticker_safety_delay_time =" << engine->getSettings()->ticker_safety_delay_time;
//    kDebug() << "slippage_stale_time =" << rest->slippage_stale_time;
//    kDebug() << "orderbook_stale_tolerance =" << rest->orderbook_stale_tolerance;

//    kDebug() << "nam interval =" << rest->send_timer->interval();
//    kDebug() << "orderbook update interval =" << rest->orderbook_timer->interval();

//    kDebug() << "ticker interval =" << rest->ticker_timer->interval();
//    kDebug() << "timeout interval =" << rest->timeout_timer->interval();
//    kDebug() << "dc interval =" << rest->diverge_converge_timer->interval();
//    kDebug() << "is_chatty = " << engine->getSettings()->is_chatty;
}

void CommandRunner::command_getinternal( QStringList &args )
{
    Q_UNUSED( args )
    engine->printInternal();

//    kDebug() << "nam_queue size:" << rest->nam_queue.size();
//    kDebug() << "nam_queue_sent size:" << rest->nam_queue_sent.size();
//    kDebug() << "orderbook_update_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_update_time ).toString();
//    kDebug() << "orderbook_update_request_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_update_request_time ).toString();
//    kDebug() << "orderbook_public_update_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_public_update_time ).toString();
//    kDebug() << "orderbook_public_update_request_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_public_update_request_time ).toString();


//    kDebug() << "avg response time:" << rest->avg_response_time.avgResponseTime();
//    kDebug() << "orders_stale_trip_count: " << rest->orders_stale_trip_count;
//    kDebug() << "books_stale_trip_count: " << rest->books_stale_trip_count;
//    kDebug() << "nonce:" << rest->request_nonce;

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

void CommandRunner::command_clearallstats( QStringList &args )
{
    Q_UNUSED( args )
    //stats->clearAll();
}

void CommandRunner::command_savemarket( QStringList &args )
{
    engine->saveMarket( Market( args.value( 1 ) ), args.value( 2 ).toInt() );
}

void CommandRunner::command_savesettings( QStringList &args )
{
    Q_UNUSED( args )
    spruce_overseer->saveSettings();
}

void CommandRunner::command_savestats( QStringList &args )
{
    Q_UNUSED( args )
    spruce_overseer->saveStats();
}

void CommandRunner::command_sendcommand( QStringList &args )
{
    Q_UNUSED( args )
    //rest->sendRequest( args.value( 1 ), args.value( 2 ) );
}

void CommandRunner::command_setchatty( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    bool chatty = args.value( 1 ) == "true" ? true : false;

    engine->getSettings()->is_chatty = chatty;
    kDebug() << "is_chatty set to" << chatty;
}

void CommandRunner::command_exit( QStringList &args )
{
    Q_UNUSED( args )
    emit exitSignal();
}

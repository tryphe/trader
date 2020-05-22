#include "commandrunner.h"
#include "global.h"
#include "engine.h"
#include "enginesettings.h"
#include "trexrest.h"
#include "polorest.h"
#include "wavesrest.h"
#include "bncrest.h"
#include "positionman.h"
#include "market.h"
#include "alphatracker.h"
#include "spruce.h"
#include "spruceoverseer.h"

#include <functional>
#include <QString>
#include <QMap>
#include <QVector>
#include <QQueue>
#include <QTimer>

CommandRunner::CommandRunner( const quint8 _engine_type, Engine *_e, QVector<BaseREST*> _rest_arr, QObject *parent )
    : QObject( parent ),
      rest_arr( _rest_arr ),
      engine_type( _engine_type ),
      engine( _e )
{
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
    command_map.insert( "setalphamanual", std::bind( &CommandRunner::command_setalphamanual, this, _1 ) );
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
    command_map.insert( "setspruceinterval", std::bind( &CommandRunner::command_setspruceinterval, this, _1 ) );
    command_map.insert( "setsprucebasecurrency", std::bind( &CommandRunner::command_setsprucebasecurrency, this, _1 ) );
    command_map.insert( "setspruceweight", std::bind( &CommandRunner::command_setspruceweight, this, _1 ) );
    command_map.insert( "setsprucestartnode", std::bind( &CommandRunner::command_setsprucestartnode, this, _1 ) );
    command_map.insert( "setspruceshortlongtotal", std::bind( &CommandRunner::command_setspruceshortlongtotal, this, _1 ) );
    command_map.insert( "setsprucebetamarket", std::bind( &CommandRunner::command_setsprucebetamarket, this, _1 ) );
    command_map.insert( "setspruceamplification", std::bind( &CommandRunner::command_setspruceamplification, this, _1 ) );
    command_map.insert( "setspruceprofile", std::bind( &CommandRunner::command_setspruceprofile, this, _1 ) );
    command_map.insert( "setsprucereserve", std::bind( &CommandRunner::command_setsprucereserve, this, _1 ) );
    command_map.insert( "setspruceordergreed", std::bind( &CommandRunner::command_setspruceordergreed, this, _1 ) );
    command_map.insert( "setspruceordersize", std::bind( &CommandRunner::command_setspruceordersize, this, _1 ) );
    command_map.insert( "setspruceordercount", std::bind( &CommandRunner::command_setspruceordercount, this, _1 ) );
    command_map.insert( "setspruceordertimeout", std::bind( &CommandRunner::command_setspruceordertimeout, this, _1 ) );
    command_map.insert( "setspruceordernice", std::bind( &CommandRunner::command_setspruceordernice, this, _1 ) );
    command_map.insert( "setspruceordernicecustom", std::bind( &CommandRunner::command_setspruceordernicecustom, this, _1 ) );
    command_map.insert( "setspruceordernicemarketoffset", std::bind( &CommandRunner::command_setspruceordernicemarketoffset, this, _1 ) );
    command_map.insert( "setspruceallocation", std::bind( &CommandRunner::command_setspruceallocation, this, _1 ) );
    command_map.insert( "setsprucesnapback", std::bind( &CommandRunner::command_setsprucesnapback, this, _1 ) );
    command_map.insert( "setsprucesnapbacktrigger1", std::bind( &CommandRunner::command_setsprucesnapbacktrigger1, this, _1 ) );
    command_map.insert( "setsprucesnapbacktrigger2", std::bind( &CommandRunner::command_setsprucesnapbacktrigger2, this, _1 ) );
    command_map.insert( "getmidspreadstatus", std::bind( &CommandRunner::command_getmidspreadstatus, this, _1 ) );
    command_map.insert( "getstatus", std::bind( &CommandRunner::command_getstatus, this, _1 ) );
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

    // count the number of non-empty args
    qint32 nonzero_args_size = 0;
    for ( QStringList::const_iterator i = args.begin(); i != args.end(); i++ )
        if ( !(*i).trimmed().isEmpty() )
            nonzero_args_size++;

    // check for expected arg count
    if ( nonzero_args_size < expected_args_min || nonzero_args_size > expected_args_max )
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
        rest_arr.at( ENGINE_BITTREX )->sendRequest( TREX_COMMAND_GET_BALANCES );
    else if ( engine_type == ENGINE_BINANCE )
        rest_arr.at( ENGINE_BINANCE )->sendRequest( BNC_COMMAND_GETBALANCES, "", nullptr, 5 );
    else if ( engine_type == ENGINE_POLONIEX )
        rest_arr.at( ENGINE_POLONIEX )->sendRequest( POLO_COMMAND_GETBALANCES );
}

void CommandRunner::command_getlastprices( QStringList & )
{
    //stats->printLastPrices();
}

void CommandRunner::command_getbuyselltotal( QStringList & )
{
    QMap<QString /*market*/, qint32> buy_count, sell_count, total_count;
    QMap<QString /*market*/, Coin> buy_amount, sell_qty, total_amount;
    qint32 buy_overall_count = 0, sell_overall_count = 0, total_overall_count = 0;
    Coin buy_overall_amount, sell_overall_amount, total_overall_amount;

    QString base_asset = spruce_overseer->spruce->getBaseCurrency();

    // build indexes from active and queued positions
    QSet<Position*>::const_iterator begin = engine->getPositionMan()->all().begin(),
                                    end = engine->getPositionMan()->all().end();
    for ( QSet<Position*>::const_iterator i = begin; i != end; i++ )
    {
        Position *const &pos = *i;

        quint8 side_actual;
        Market market_actual;
        Coin price_actual, amount_actual, quantity_actual;

        /// step 1: filter side, market, price, amount, qty by base spruce base market
        // quote asset = spruce base, invert market
        if ( base_asset == pos->market.getQuote() )
        {
            side_actual = ( pos->side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY;
            market_actual = pos->market.getInverse();
            price_actual = CoinAmount::COIN / pos->price;
            amount_actual = pos->quantity;
            quantity_actual = pos->amount;
        }
        // base == spruce base, or neither of the currencies are the base(TODO: beta market conversions)
        else// if ( base_asset == pos->market.getBase() )
        {
            side_actual = pos->side;
            market_actual = pos->market;
            price_actual = pos->price;
            amount_actual = pos->amount;
            quantity_actual = pos->quantity;
        }

        /// step 2: fill maps with data
        if ( side_actual == SIDE_BUY )
        {
            buy_count[ market_actual ]++;
            buy_amount[ market_actual ] += amount_actual;
            buy_overall_count++;
            buy_overall_amount += amount_actual;
        }
        else
        {
            sell_count[ market_actual ]++;
            sell_qty[ market_actual ] += quantity_actual;
            sell_overall_count++;
            sell_overall_amount += amount_actual;
        }

        // save the total count
        total_count[ market_actual ]++;
        total_overall_count++;
        total_amount[ market_actual ] += amount_actual;
        total_overall_amount += amount_actual;
    }

    // print header
    // spacing:           10   5    5    5    17   17   17
    kDebug() << QString( "%1 | %2 | %3 | %4 | %5 | %6 | %7" )
                .arg( QString(), MARKET_STRING_WIDTH )
                .arg( "buys", -5 )
                .arg( "sells", -5 )
                .arg( "total", -5 )
                .arg( "buy amt", -PRICE_WIDTH )
                .arg( "sell qty", -PRICE_WIDTH )
                .arg( "total amt", -PRICE_WIDTH ); // padding

    const QString line_str = "%1 | >>>grn<<<%2>>>none<<< | >>>red<<<%3>>>none<<< | %4 | >>>grn<<<%5>>>none<<< | >>>red<<<%6>>>none<<< | %7";

    // print each market with orders active
    for ( QMap<QString, qint32>::const_iterator i = total_count.begin(); i != total_count.end(); i++ )
    {
        const QString &market = i.key();

        kDebug() << QString( line_str )
                    .arg( market, -MARKET_STRING_WIDTH )
                    .arg( buy_count.value( market ), -5 )
                    .arg( sell_count.value( market ), -5 )
                    .arg( total_count.value( market ), -5 )
                    .arg( buy_amount.value( market ), -PRICE_WIDTH )
                    .arg( sell_qty.value( market ), -PRICE_WIDTH )
                    .arg( total_amount.value( market ), -PRICE_WIDTH );
    }

    // print total overall
    kDebug() << QString( line_str )
                .arg( QString(), -MARKET_STRING_WIDTH )
                .arg( buy_overall_count, -5 )
                .arg( sell_overall_count, -5 )
                .arg( total_overall_count, -5 )
                .arg( buy_overall_amount, -PRICE_WIDTH )
                .arg( sell_overall_amount, -PRICE_WIDTH )
                .arg( total_overall_amount, -PRICE_WIDTH );
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
    rest_arr.at( engine_type )->market_cancel_thresh = args.value( 1 ).toInt();
    kDebug() << "cancel thresh changed to" << rest_arr.at( engine_type )->market_cancel_thresh;
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

void CommandRunner::command_setalphamanual( QStringList &args )
{
    if ( !checkArgs( args, 5 ) ) return;

    const Market market = Market( args.value( 1 ) );
    const quint8 side = args.value( 2 ).toLower() == "buy" ? SIDE_BUY : SIDE_SELL;
    const Coin amt = args.value( 3 );
    const Coin qty = args.value( 4 );
    const Coin price = args.value( 5 );

    if ( !market.isValid() )
    {
        kDebug() << "setalphamanual: invalid market" << market;
        return;
    }

    if ( amt.isZeroOrLess() && qty.isZeroOrLess() )
    {
        kDebug() << "setalphamanual: invalid amount" << amt << "or quantity" << qty << "(one can be zero)";
        return;
    }

    if ( price.isZeroOrLess() )
    {
        kDebug() << "setalphamanual: invalid price" << price;
        return;
    }

    engine->updateStatsAndPrintFill( "extern", market, "external_order", side, "spruce_external", amt, qty, price, Coin() );
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
        const Coin &hi_buy  = i.value().ticker.bid;
        const Coin &lo_sell = i.value().ticker.ask;

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
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->send_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "nam interval set to" << rest_arr.at( engine_type )->send_timer->interval();
}

void CommandRunner::command_setbookinterval( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->orderbook_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "nam interval set to" << rest_arr.at( engine_type )->orderbook_timer->interval();
}

void CommandRunner::command_settickerinterval( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->ticker_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "nam interval set to" << rest_arr.at( engine_type )->ticker_timer->interval();
}

void CommandRunner::command_setgracetimelimit( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->getSettings()->stray_grace_time_limit = args.value( 1 ).toLong();
    kDebug() << "stray_grace_time_limit set to" << engine->getSettings()->stray_grace_time_limit;
}

void CommandRunner::command_setcheckinterval( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->timeout_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "nam interval set to" << rest_arr.at( engine_type )->timeout_timer->interval();
}

void CommandRunner::command_setdcinterval( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->diverge_converge_timer->setInterval( args.value( 1 ).toInt() );
    kDebug() << "nam interval set to" << rest_arr.at( engine_type )->diverge_converge_timer->interval();
}

void CommandRunner::command_setclearstrayorders( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->getSettings()->should_clear_stray_orders = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_clear_stray_orders set to" << engine->getSettings()->should_clear_stray_orders;
}

void CommandRunner::command_setclearstrayordersall( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->getSettings()->should_clear_stray_orders_all = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_clear_stray_orders_all set to" << engine->getSettings()->should_clear_stray_orders_all;
}

void CommandRunner::command_setslippagecalculated( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->getSettings()->should_slippage_be_calculated = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_slippage_be_calculated set to" << engine->getSettings()->should_slippage_be_calculated;
}

void CommandRunner::command_setadjustbuysell( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->getSettings()->should_adjust_hibuy_losell = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_adjust_hibuy_losell set to" << engine->getSettings()->should_adjust_hibuy_losell;
}

void CommandRunner::command_setdcslippage( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->getSettings()->should_dc_slippage_orders = args.value( 1 ) == "true" ? true : false;
    kDebug() << "should_dc_slippage_orders set to" << engine->getSettings()->should_dc_slippage_orders;
}

void CommandRunner::command_setorderbookstaletolerance( QStringList &args )
{
    qlonglong new_tolerance = 0;

    rest_arr.at( engine_type )->orderbook_stale_tolerance = args.value( 1 ).toLongLong();
    new_tolerance = rest_arr.at( engine_type )->orderbook_stale_tolerance;

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
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->slippage_stale_time = args.value( 1 ).toLongLong();
    kDebug() << "slippage_stale_time set to" << rest_arr.at( engine_type )->slippage_stale_time << "ms";
}

void CommandRunner::command_setqueuedcommandsmax( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->limit_commands_queued = args.value( 1 ).toInt();
    kDebug() << "limit_commands_queued set to" << rest_arr.at( engine_type )->limit_commands_queued;
}

void CommandRunner::command_setqueuedcommandsmaxdc( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->limit_commands_queued_dc_check = args.value( 1 ).toInt();
    kDebug() << "limit_commands_queued_dc_check set to" << rest_arr.at( engine_type )->limit_commands_queued_dc_check;
}

void CommandRunner::command_setsentcommandsmax( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->limit_commands_sent = args.value( 1 ).toInt();
    kDebug() << "sent commands max set to" << rest_arr.at( engine_type )->limit_commands_sent;
}

void CommandRunner::command_settimeoutyield( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    rest_arr.at( engine_type )->limit_timeout_yield = args.value( 1 ).toInt();
    kDebug() << "limit_timeout_yield set to" << rest_arr.at( engine_type )->limit_timeout_yield;
}

void CommandRunner::command_setrequesttimeout( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    engine->getSettings()->order_timeout = args.value( 1 ).toLong();
    kDebug() << "order timeout is" << engine->getSettings()->order_timeout;
}

void CommandRunner::command_setcanceltimeout( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

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

void CommandRunner::command_setspruceinterval( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    const long secs = args.value( 1 ).toLong();

    spruce_overseer->spruce->setIntervalSecs( secs );
    spruce_overseer->spruce_timer->setInterval( secs *1000 );
    kDebug() << "spruce interval is now" << spruce_overseer->spruce->getIntervalSecs() << "seconds";
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

void CommandRunner::command_setsprucebetamarket( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    const Market m = args.value( 1 );
    spruce_overseer->spruce->addMarketBeta( m );
}

void CommandRunner::command_setspruceamplification( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    spruce_overseer->spruce->setAmplification( args.value( 1 ) );
    kDebug() << "spruce log amplification is" << spruce_overseer->spruce->getAmplification();
}

void CommandRunner::command_setspruceprofile( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    spruce_overseer->spruce->setProfileU( args.value( 1 ), args.value( 2 ) );
    kDebug() << "spruce profile u for" << args.value( 1 ) << "is" << spruce_overseer->spruce->getProfileU( args.value( 1 ) );
}

void CommandRunner::command_setsprucereserve( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    spruce_overseer->spruce->setReserve( args.value( 1 ), args.value( 2 ) );
    kDebug() << "spruce reserve for" << args.value( 1 ) << "is" << spruce_overseer->spruce->getReserve( args.value( 1 ) );
}

void CommandRunner::command_setspruceordergreed( QStringList &args )
{
    if ( !checkArgs( args, 4 ) ) return;

    spruce_overseer->spruce->setOrderGreed( args.value( 1 ) );
    spruce_overseer->spruce->setOrderGreedMinimum( args.value( 2 ) );
    spruce_overseer->spruce->setOrderRandomBuy( args.value( 3 ) );
    spruce_overseer->spruce->setOrderRandomSell( args.value( 4 ) );

    kDebug() << "spruce order greed:" << spruce_overseer->spruce->getOrderGreed()
             << "greed minimum:" << spruce_overseer->spruce->getOrderGreedMinimum()
             << "buy random:" << spruce_overseer->spruce->getOrderRandomBuy()
             << "sell random:" << spruce_overseer->spruce->getOrderRandomSell();
}

void CommandRunner::command_setspruceordersize( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    spruce_overseer->spruce->setOrderSize( args.value( 1 ) );
    kDebug() << "spruce ordersize is" << spruce_overseer->spruce->getOrderSize();
}

void CommandRunner::command_setspruceordercount( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    spruce_overseer->spruce->setOrdersPerSideFlux( args.value( 1 ).toUShort() );
    spruce_overseer->spruce->setOrdersPerSideMidspread( args.value( 2 ).toUShort() );

    kDebug() << "spruce order count for flux:" << spruce_overseer->spruce->getOrdersPerSideFlux()
             << "midspread:" << spruce_overseer->spruce->getOrdersPerSideMidspread();
}

void CommandRunner::command_setspruceordertimeout( QStringList &args )
{
    if ( !checkArgs( args, 3 ) ) return;

    spruce_overseer->spruce->setOrderTimeoutFlux( args.value( 1 ).toUShort(), args.value( 2 ).toUShort() );
    spruce_overseer->spruce->setOrderTimeoutMidspread( args.value( 3 ).toUShort() );

    kDebug() << "spruce order timeout for flux:" << spruce_overseer->spruce->getOrderTimeoutFlux()
             << "midspread:" << spruce_overseer->spruce->getOrderTimeoutMidspread();
}

void CommandRunner::command_setspruceordernice( QStringList &args )
{
    if ( !checkArgs( args, 6 ) ) return;

    spruce_overseer->spruce->setOrderNice( SIDE_BUY, args.value( 1 ), false );
    spruce_overseer->spruce->setOrderNiceZeroBound( SIDE_BUY, args.value( 2 ), false );
    spruce_overseer->spruce->setOrderNiceSpreadPut( SIDE_BUY, args.value( 3 ) );

    spruce_overseer->spruce->setOrderNice( SIDE_SELL, args.value( 4 ), false );
    spruce_overseer->spruce->setOrderNiceZeroBound( SIDE_SELL, args.value( 5 ), false );
    spruce_overseer->spruce->setOrderNiceSpreadPut( SIDE_SELL, args.value( 6 ) );

    kDebug()    << "buy nice:" << spruce_overseer->spruce->getOrderNice( QString(), SIDE_BUY, false )
          << "buy zero bound:" << spruce_overseer->spruce->getOrderNiceZeroBound( QString(), SIDE_BUY, false )
       << "buy spread reduce:" << spruce_overseer->spruce->getOrderNiceSpreadPut( SIDE_BUY )
               << "sell nice:" << spruce_overseer->spruce->getOrderNice( QString(), SIDE_SELL, false )
         << "sell zero bound:" << spruce_overseer->spruce->getOrderNiceZeroBound( QString(), SIDE_SELL, false )
      << "sell spread reduce:" << spruce_overseer->spruce->getOrderNiceSpreadPut( SIDE_SELL );
}

void CommandRunner::command_setspruceordernicecustom( QStringList &args )
{
    if ( !checkArgs( args, 4 ) ) return;

    spruce_overseer->spruce->setOrderNice( SIDE_BUY, args.value( 1 ), true );
    spruce_overseer->spruce->setOrderNiceZeroBound( SIDE_BUY, args.value( 2 ), true );

    spruce_overseer->spruce->setOrderNice( SIDE_SELL, args.value( 3 ), true );
    spruce_overseer->spruce->setOrderNiceZeroBound( SIDE_SELL, args.value( 4 ), true );

    kDebug() << "buy nice custom:" << spruce_overseer->spruce->getOrderNice( QString(), SIDE_BUY, true )
       << "buy zero bound custom:" << spruce_overseer->spruce->getOrderNiceZeroBound( QString(), SIDE_BUY, true )
            << "sell nice custom:" << spruce_overseer->spruce->getOrderNice( QString(), SIDE_SELL, true )
      << "sell zero bound custom:" << spruce_overseer->spruce->getOrderNiceZeroBound( QString(), SIDE_SELL, true );
}

void CommandRunner::command_setspruceordernicemarketoffset( QStringList &args )
{
    if ( !checkArgs( args, 5 ) ) return;

    spruce_overseer->spruce->setOrderNiceMarketOffset( args.value( 1 ), SIDE_BUY, args.value( 2 ) );
    spruce_overseer->spruce->setOrderNiceZeroBoundMarketOffset( args.value( 1 ), SIDE_BUY, args.value( 3 ) );
    spruce_overseer->spruce->setOrderNiceMarketOffset( args.value( 1 ), SIDE_SELL, args.value( 4 ) );
    spruce_overseer->spruce->setOrderNiceZeroBoundMarketOffset( args.value( 1 ), SIDE_SELL, args.value( 5 ) );

    kDebug() << "buy nice offset:" << spruce_overseer->spruce->getOrderNiceMarketOffset( args.value( 1 ), SIDE_BUY )
       << "buy zero bound offset:" << spruce_overseer->spruce->getOrderNiceZeroBoundMarketOffset( args.value( 1 ), SIDE_BUY )
            << "sell nice offset:" << spruce_overseer->spruce->getOrderNiceMarketOffset( args.value( 1 ), SIDE_SELL )
      << "sell zero bound offset:" << spruce_overseer->spruce->getOrderNiceZeroBoundMarketOffset( args.value( 1 ), SIDE_SELL );
}

void CommandRunner::command_setspruceallocation( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    spruce_overseer->spruce->setExchangeAllocation( args.value( 1 ),
                                                    Coin( args.value( 2 ) ) );
}

void CommandRunner::command_setsprucesnapback( QStringList &args )
{
    if ( !checkArgs( args, 1 ) ) return;

    spruce_overseer->spruce->setSnapbackRatio( args.value( 1 ) );

    kDebug() << "spruce snapback ratio:" << spruce_overseer->spruce->getSnapbackRatio();
}

void CommandRunner::command_setsprucesnapbacktrigger1( QStringList &args )
{
    if ( !checkArgs( args, 2 ) ) return;

    spruce_overseer->spruce->setSnapbackTrigger1Window( args.value( 1 ).toLongLong() );
    spruce_overseer->spruce->setSnapbackTrigger1Iterations( args.value( 2 ).toLongLong() );

    kDebug() << "spruce snapback trigger 1 window:" << spruce_overseer->spruce->getSnapbackTrigger1Window()
                                   << "iterations:" << spruce_overseer->spruce->getSnapbackTrigger1Iterations();
}

void CommandRunner::command_setsprucesnapbacktrigger2( QStringList &args )
{
    if ( !checkArgs( args, 4 ) ) return;

    spruce_overseer->spruce->setSnapbackTrigger2MASamples( args.value( 1 ).toLong() );
    spruce_overseer->spruce->setSnapbackTrigger2MARatio( args.value( 2 ) );
    spruce_overseer->spruce->setSnapbackTrigger2InitialRatio( args.value( 3 ) );
    spruce_overseer->spruce->setSnapbackTrigger2MessageInterval( args.value( 4 ).toLongLong() );

    kDebug() << "spruce snapback trigger 2 ma samples:" << spruce_overseer->spruce->getSnapbackTrigger2MASamples()
                                         << "ma ratio:" << spruce_overseer->spruce->getSnapbackTrigger2MARatio()
                                    << "initial ratio:" << spruce_overseer->spruce->getSnapbackTrigger2InitialRatio()
                                 << "message interval:" << spruce_overseer->spruce->getSnapbackTrigger2MessageInterval();
}

void CommandRunner::command_spruceup( QStringList & )
{
    spruce_overseer->onSpruceUp();
}

void CommandRunner::command_getmidspreadstatus( QStringList &args )
{
    Q_UNUSED( args )

    QString midspread_state = spruce_overseer->getLastMidspreadPhaseOutput();
    midspread_state.chop( 1 ); // chop the last line break

    kDebug() << QString( "Last midspread state:\n%1" ).arg( midspread_state );
}

void CommandRunner::command_getstatus( QStringList &args )
{
    Q_UNUSED( args )

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    const qint64 time_thresh = current_time -
                              ( BITTREX_TIMER_INTERVAL_TICKER +
                                BINANCE_TIMER_INTERVAL_TICKER +
                                POLONIEX_TIMER_INTERVAL_TICKER );

    QString ticker_status;

    for ( int i = 0; i < rest_arr.size(); i++ )
    {
        if ( rest_arr.at( i ) != nullptr )
            ticker_status += QString( "%1%2%3 | " )
                             .arg( rest_arr.at( i )->ticker_update_time > time_thresh ? COLOR_GREEN : COLOR_RED )
                             .arg( rest_arr.at( i )->exchange_string )
                             .arg( COLOR_NONE );
    }

    if ( ticker_status.size() > 0 )
        ticker_status.chop( 3 );

    kDebug() << ticker_status;
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

//    kDebug() << "limit_commands_queued =" << rest->limit_commands_queued;
//    kDebug() << "limit_commands_queued_dc_check =" << rest->limit_commands_queued_dc_check;
//    kDebug() << "limit_commands_sent =" << rest->limit_commands_sent;
//    kDebug() << "limit_timeout_yield =" << rest->limit_timeout_yield;
//    kDebug() << "market_cancel_thresh =" << rest->market_cancel_thresh;
//    kDebug() << "order_timeout =" << engine->getSettings()->order_timeout;
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

    BaseREST *rest = rest_arr.value( engine_type );

    kDebug() << "nam_queue size:" << rest->nam_queue.size();
    kDebug() << "nam_queue_sent size:" << rest->nam_queue_sent.size();
    kDebug() << "orderbook_update_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_update_time ).toString();
    kDebug() << "orderbook_update_request_time:" << QDateTime::fromMSecsSinceEpoch( rest->orderbook_update_request_time ).toString();
    kDebug() << "ticker_update_time:" << QDateTime::fromMSecsSinceEpoch( rest->ticker_update_time ).toString();
    kDebug() << "ticker_update_request_time:" << QDateTime::fromMSecsSinceEpoch( rest->ticker_update_request_time ).toString();
    kDebug() << "avg response time:" << rest->avg_response_time.avgResponseTime();
    kDebug() << "orders_stale_trip_count: " << rest->orders_stale_trip_count;
    kDebug() << "books_stale_trip_count: " << rest->books_stale_trip_count;
    kDebug() << "request nonce:" << rest->request_nonce;
    kDebug() << Global::getBuildString();

    kDebug() << spruce_overseer->spruce->getMarketsBeta();
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

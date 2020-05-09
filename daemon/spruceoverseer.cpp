#include "spruceoverseer.h"
#include "spruce.h"
#include "alphatracker.h"
#include "position.h"
#include "coinamount.h"
#include "misctypes.h"
#include "market.h"
#include "engine.h"
#include "positionman.h"

#include <QTimer>
#include <QVector>
#include <QMap>
#include <QList>
#include <QSet>
#include <QFile>

const bool expand_spread_base_down = false; // true = getSpreadForSide always expands down for base greed value before applying other effects
const bool expand_spread_buys = false; // expand buy side down more than sell side

const bool prices_uses_avg = true; // false = assemble widest combined spread between all exchanges, true = average spreads between all exchanges

static const QString MIDSPREAD_PHASE = "mid_0";

SpruceOverseer::SpruceOverseer( Spruce *_spruce )
    : QObject( nullptr ),
    spruce( _spruce )
{
    kDebug() << "[SpruceOverseer]";

    // this timer does tit-for-tat
    spruce_timer = new QTimer( this );
    connect( spruce_timer, &QTimer::timeout, this, &SpruceOverseer::onSpruceUp );
    spruce_timer->setTimerType( Qt::VeryCoarseTimer );
    spruce_timer->start( spruce->getIntervalSecs() * 1000 );

    // autosave spruce settings
    autosave_timer = new QTimer( this );
    connect( autosave_timer, &QTimer::timeout, this, &SpruceOverseer::onSaveSpruceSettings );
    autosave_timer->setTimerType( Qt::VeryCoarseTimer );
    autosave_timer->start( 60000 * 60 ); // set default to 1hr
}

SpruceOverseer::~SpruceOverseer()
{
    spruce_timer->stop();
    autosave_timer->stop();

    delete spruce_timer;
    delete autosave_timer;
}

void SpruceOverseer::onSpruceUp()
{
    // store last spread distance limits
    static QMap<QString,Coin> last_spread_reduce_buys;
    static QMap<QString,Coin> last_spread_reduce_sells;

    if ( !spruce->isActive() )
        return;

    QMap<QString/*market*/,Coin> spread_price;
    const QList<QString> currencies = spruce->getCurrencies();
    QList<QString> markets;
    markets += MIDSPREAD_PHASE; // 1 phase for middle spread
    markets += spruce->getMarketsAlpha(); // 1 phase for each market

    for ( QList<QString>::const_iterator m = markets.begin(); m != markets.end(); m++ )
    {
        // track mid spread for each market (spread for every market is needed for custom phase)
        QMap<QString,TickerInfo> mid_spread;

        const Market market_phase = *m;

        // one pass in each phase for buys and sells
        for ( quint8 side = SIDE_BUY; side < SIDE_SELL +1; side++ )
        {
            const QString phase_name = QString( "spruce-%1-%2" )
                                      .arg( side == SIDE_BUY ? "B" : "S" )
                                      .arg( market_phase );

            // initialize duplicity spread (used only after custom phase)
            TickerInfo spread_duplicity;
            if ( market_phase != MIDSPREAD_PHASE )
            {
                spread_duplicity = getSpreadForSide( market_phase, side, true, false, true, true );

                if ( !spread_duplicity.isValid() )
                {
                    kDebug() << "spruceoverseer error: duplicity spread was not valid for phase" << phase_name;
                    return;
                }
            }

            spruce->clearLiveNodes();
            for ( QList<QString>::const_iterator i = currencies.begin(); i != currencies.end(); i++ )
            {
                const QString &currency = *i;
                const Market market( spruce->getBaseCurrency(), currency );

                // if midspread is blank, cache it
                if ( !mid_spread.contains( market ) )
                {
                    mid_spread[ market ] = getMidSpread( market );

                    if ( !mid_spread.value( market ).isValid() )
                    {
                        kDebug() << "spruceoverseer error: midspread was not valid for phase" << phase_name;
                        return;
                    }
                }

                Coin flux_price = ( side == SIDE_BUY ) ? mid_spread.value( market ).bid :
                                                         mid_spread.value( market ).ask;

                // these prices should be equal
                if ( mid_spread.value( market ).bid != mid_spread.value( market ).ask )
                {
                    kDebug() << "local error: midspread bid" << mid_spread.value( market ).bid << "!= ask" << mid_spread.value( market ).ask;
                    return;
                }

                // if the ticker isn't updated, just skip this whole function
                if ( flux_price.isZeroOrLess() )
                {
                    kDebug() << "[Spruce] local error: no ticker for currency" << market;
                    return;
                }

                // if market matches selected market, select best price from duplicity price or mid price
                if ( market == market_phase &&
                     market_phase != MIDSPREAD_PHASE ) // on custom iteration, skip this step
                {
                    // set the most optimistic price to use, either the midprice or duplicity price
                    flux_price = ( side == SIDE_BUY ) ? std::min( flux_price, spread_duplicity.bid ) :
                                                        std::max( flux_price, spread_duplicity.ask );
                }

                spread_price.insert( currency, flux_price );
                spruce->addLiveNode( currency, flux_price );
            }

            // on the sell side of custom iteration 0, the result is the same as the buy side, so skip it
            if ( !( side == SIDE_SELL && market_phase == MIDSPREAD_PHASE ) )
            {
                // calculate amount to short/long, and fail if necessary
                if ( !spruce->calculateAmountToShortLong() )
                    return;
            }

            const QMap<QString,Coin> &qty_to_shortlong_map = spruce->getQuantityToShortLongMap();

            for ( QMap<quint8, Engine*>::const_iterator e = engine_map.begin(); e != engine_map.end(); e++ )
            {
                Engine *engine = e.value();

                for ( QMap<QString,Coin>::const_iterator i = qty_to_shortlong_map.begin(); i != qty_to_shortlong_map.end(); i++ )
                {
                    const QString &market = i.key();

                    // skip market unless it's selected
                    if ( market != market_phase &&
                         market_phase != MIDSPREAD_PHASE ) // on custom iteration, set an order for every market
                        continue;

                    const QString exchange_market_key = QString( "%1-%2" )
                                                        .arg( e.key() )
                                                        .arg( market );

                    // get market allocation for this exchange and apply to qty_to_shortlong
                    const Coin market_allocation = spruce->getExchangeAllocation( exchange_market_key );

                    // continue on zero market allocation for this engine
                    if ( market_allocation.isZeroOrLess() )
                        continue;

                    QString order_type = "onetime";
                    Coin buy_price, sell_price;

                    // set price for order
                    if ( market_phase == MIDSPREAD_PHASE )
                    {
                        buy_price = mid_spread.value( market ).bid;
                        sell_price = mid_spread.value( market ).ask;

                        // set local taker mode to disable local spread collision detection which would modify the price
                        order_type += "-taker";

                        // set fast timeout
                        order_type += "-timeout5";
                    }
                    else
                    {
                        buy_price = spread_duplicity.bid;
                        sell_price = spread_duplicity.ask;
                    }

                    // run cancellors for this phase every iteration
                    const Coin cancel_thresh_price = ( market_phase == MIDSPREAD_PHASE ) ? Coin() :
                                                     ( side == SIDE_BUY ) ? buy_price : sell_price;
                    runCancellors( engine, market, side, phase_name, cancel_thresh_price );

                    const Coin qty_to_shortlong = i.value() * market_allocation;
                    const bool is_buy = qty_to_shortlong.isZeroOrLess();

                    // don't place buys during the ask price loop, or sells during the bid price loop
                    if ( (  is_buy && side == SIDE_SELL ) ||
                         ( !is_buy && side == SIDE_BUY ) )
                        continue;

                    // cache some order settings
                    const Coin order_size_default = spruce->getOrderSize( market );
                    const Coin order_nice = spruce->getOrderNice( market, side, market_phase == MIDSPREAD_PHASE );
                    const Coin order_size_limit = order_size_default * order_nice;

                    // cache amount to short/long
                    const Coin amount_to_shortlong = spruce->getCurrencyPriceByMarket( market ) * qty_to_shortlong;
                    const Coin amount_to_shortlong_abs = amount_to_shortlong.abs();

                    // if we're under the nice size limit, skip conflict checks and order setting
                    if ( amount_to_shortlong_abs < order_size_limit )
                        continue;

                    // we're over the nice value for the midspread phase.
                    // this will modify nice values for all phases on this side on the next round of onSpruceUp() call to getOrderNice()
                    if (  market_phase == MIDSPREAD_PHASE &&
                         !spruce->getSnapbackState( market, side ) )
                    {
                        spruce->setSnapbackState( market, side, true );
                    }

                    Coin spread_distance_limit;

                    /// for duplicity phases, detect conflicting positions for this market within the spread distance limit
                    if ( market_phase != MIDSPREAD_PHASE )
                    {
                        const Coin spread_put_threshold = order_size_default * spruce->getOrderNiceSpreadPut( side );

                        // declare spread reduce here so we can print/evalulate it after
                        Coin spread_reduce;

                        // reduce spread if pending amount to shortlong is greater than size * order_nice_spreadput
                        if ( spread_put_threshold.isGreaterThanZero() &&
                             amount_to_shortlong_abs > spread_put_threshold )
                        {
                            spread_reduce = ( amount_to_shortlong_abs / spread_put_threshold ) * ( CoinAmount::SATOSHI * 100000 );

                            const TickerInfo collapsed_spread = getSpreadForSide( market, side, true, false, true, true, spread_reduce );
                            if ( collapsed_spread.isValid() )
                            {
                                buy_price = collapsed_spread.bid;
                                sell_price = collapsed_spread.ask;
                            }
                            else
                            {
                                kDebug() << "spruceoverseer error: collapsed spread was not valid for phase" << phase_name;
                                return;
                            }
                        }

                        // cache spread distance limit for this side
                        if ( is_buy )
                            last_spread_reduce_buys.insert( market, spread_reduce );
                        else
                            last_spread_reduce_sells.insert( market, spread_reduce );

                        // cache spread distance limit for this side, but selected larger spread_reduce value from both sides
                        const Coin spread_reduce_selected = std::max( last_spread_reduce_buys.value( market ), last_spread_reduce_sells.value( market ) );
                        spread_distance_limit = std::min( spruce->getOrderGreed() + spread_reduce_selected, spruce->getOrderGreedMinimum() );

                        // search positions for conflicts
                        for ( QSet<Position*>::const_iterator j = engine->positions->all().begin(); j != engine->positions->all().end(); j++ )
                        {
                            Position *const &pos = *j;

                            // look for positions on the other side of this market
                            if ( pos->side == side ||
                                 pos->is_cancelling ||
                                 pos->order_set_time == 0 ||
                                 pos->market != market ||
                                !pos->strategy_tag.endsWith( market ) )
                                continue;

                            if ( (  is_buy && buy_price >= pos->sell_price * spread_distance_limit ) ||
                                 ( !is_buy && sell_price * spread_distance_limit <= pos->buy_price ) )
                            {
                                engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_CONFLICT );
                            }
                        }
                    }

                    // set slow timeout
                    if ( !order_type.contains( "timeout" ) )
                        order_type += QString( "-timeout%1" )
                                      .arg( Global::getSecureRandomRange32( 60, 90 ) );

                    // check amount active
                    const Coin spruce_active_for_side = engine->positions->getActiveSpruceEquityTotal( market, phase_name, side, Coin() );
                    //const Coin spruce_active_for_side_up_to_flux_price = engine->positions->getActiveSpruceEquityTotal( market, side, price_to_use );

                    // calculate order size, prevent going over amount_to_shortlong_abs but also prevent going under order_size_default
                    const int ORDER_CHUNKS_ESTIMATE_PER_SIDE = 9;
                    const int ORDER_SCALING_PHASE_0 = 3;
                    const Coin order_size = ( market_phase == MIDSPREAD_PHASE ) ?
                                std::max( order_size_default * ORDER_SCALING_PHASE_0, ( amount_to_shortlong_abs - order_size_limit - spruce_active_for_side ) / ORDER_SCALING_PHASE_0 ) :
                                std::min( std::max( order_size_default, amount_to_shortlong_abs - spruce_active_for_side ),
                                          std::max( order_size_default, amount_to_shortlong_abs / ORDER_CHUNKS_ESTIMATE_PER_SIDE ) );

                    // don't go under the default order size
                    if ( order_size < order_size_default )
                        continue;

                    // don't go over the abs value of our new projected position, and also regard nice value
                    // TODO: once we use smoothing for sp3 calculation, use spruce_active_for_side_up_to_flux_price
                    if ( spruce_active_for_side + order_size > amount_to_shortlong_abs ||
                         spruce_active_for_side + order_size_limit > amount_to_shortlong_abs )
                        continue;

                    kDebug() << QString( "[%1 %2] %3 | co %4 | q %5 | a %6[%7] | act %8 | dst %9" )
                                   .arg( phase_name, -MARKET_STRING_WIDTH - 9 )
                                   .arg( market, MARKET_STRING_WIDTH )
                                   .arg( side == SIDE_BUY ? buy_price : sell_price )
                                   .arg( spruce->getLastCoeffForMarket( market ).toString( 4 ), 7 )
                                   .arg( qty_to_shortlong, 16 )
                                   .arg( amount_to_shortlong, 13 )
                                   .arg( amount_to_shortlong + ( is_buy ? order_size_limit : -order_size_limit ), 13 )
                                   .arg( spruce_active_for_side, 12 )
                                   .arg( spread_distance_limit.toString( 4 ) );

                    // queue the order if we aren't paper trading
#if !defined(PAPER_TRADE)
                    engine->addPosition( market, is_buy ? SIDE_BUY : SIDE_SELL, buy_price, sell_price, order_size,
                                         order_type, phase_name, QVector<qint32>(), false, true );
#endif
                }
            }
        }
    }
}

void SpruceOverseer::adjustSpread( TickerInfo &spread, Coin limit, quint8 side, Coin &minimum_ticksize, bool expand )
{
    // get price ticksize
    Coin ticksize = minimum_ticksize;

    adjustTicksizeToSpread( ticksize, spread, minimum_ticksize );

    static const Coin CONTRACT_RATIO = Coin( SPREAD_CONTRACT_RATIO );
    static Coin diff_threshold;
    static Coin moved;

    // if contracting, calculate the contraction limit
    if ( !expand )
    {
        moved = Coin();
        ticksize = -ticksize;

        const Coin diff = std::max( spread.bid, spread.ask ) - std::min( spread.bid, spread.ask );
        diff_threshold = diff * CONTRACT_RATIO;
    }

    quint32 j = ( side == SIDE_BUY ) ? 0 : 1;

    while ( expand ? spread.bid > spread.ask * limit :
                     spread.bid < spread.ask * limit )
    {
        // if the side is buy, expand down, otherwise expand outwards
        j++;
        if ( ( side == SIDE_BUY && expand_spread_buys && j % 4 < 3 ) || // if expanding down, only expand down 60% of the time
             ( j % 2 == 1 ) ) // if not expanding down, expand 50/50
            spread.bid -= ticksize;
        else
            spread.ask += ticksize;

        // only collapse the spread by up to half the distance
        if ( !expand )
        {
            moved += ticksize;
            if ( moved >= diff_threshold )
                break;
        }
    }
}

void SpruceOverseer::adjustTicksizeToSpread( Coin &ticksize, TickerInfo &spread, const Coin &ticksize_minimum )
{
    const Coin diff = std::max( spread.bid, spread.ask ) - std::min( spread.bid, spread.ask );

    // clamp adjustSpread() to 1000 iterations
    if ( diff > ticksize * 1000 )
        ticksize = std::max( diff / 1000, ticksize_minimum );
}

TickerInfo SpruceOverseer::getSpreadLimit( const QString &market, bool order_duplicity )
{
    // get trailing limit for this side
    const Coin trailing_limit_buy = spruce->getOrderTrailingLimit( SIDE_BUY );
    const Coin trailing_limit_sell = spruce->getOrderTrailingLimit( SIDE_SELL );

    // get price ticksize
    const Coin ticksize_minimum = getPriceTicksizeForMarket( market );
    Coin ticksize = ticksize_minimum;

    // read combined spread from all exchanges. include limts for side, but don't randomize
    TickerInfo ticker_buy = getSpreadForSide( market, SIDE_BUY, order_duplicity, false, true );
    TickerInfo ticker_sell = getSpreadForSide( market, SIDE_SELL, order_duplicity, false, true );

    // first, vibrate one way
    TickerInfo spread1 = TickerInfo( ticker_buy.bid, ticker_buy.ask );

    // adjust ticksize
    adjustTicksizeToSpread( ticksize, ticker_buy, ticksize_minimum );

    // expand spread
    adjustSpread( spread1, trailing_limit_buy, SIDE_BUY, ticksize );

    // vibrate the other way
    TickerInfo spread2 = TickerInfo( ticker_sell.bid, ticker_sell.ask );

    // adjust ticksize
    adjustTicksizeToSpread( ticksize, ticker_buy, ticksize_minimum );

    // expand spread
    adjustSpread( spread2, trailing_limit_sell, SIDE_SELL, ticksize );

    // combine vibrations
    TickerInfo combined_spread = TickerInfo( spread1.bid, spread2.ask );

    if ( !order_duplicity )
    {
        const Coin midprice = combined_spread.getMidPrice();
        combined_spread.bid = midprice;
        combined_spread.ask = midprice;
    }

    //kDebug() << "spread for" << market << ":" << combined_spread;

    return combined_spread;
}

TickerInfo SpruceOverseer::getMidSpread( const QString &market )
{
    TickerInfo ret;
    quint16 samples = 0;

    for ( QMap<quint8, Engine*>::const_iterator i = engine_map.begin(); i != engine_map.end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->market_info.contains( market ) )
            continue;

        // ensure ticker isn't stale
        if ( engine->rest_arr.at( engine->engine_type )->ticker_update_time
             < QDateTime::currentMSecsSinceEpoch() - 60000 )
        {
            //kDebug() << "local warning: engine ticker" << engine->engine_type << "is stale, skipping";
            continue;
        }

        const MarketInfo &info = engine->market_info[ market ];

        // ensure prices are valid
        if ( !info.ticker.isValid() )
            continue;

        // use avg spread
        if ( prices_uses_avg )
        {
            samples++;

            // incorporate prices of this exchange
            ret.bid += info.ticker.bid;
            ret.ask += info.ticker.ask;
        }
        // or, use combined spread edges
        else
        {
            // incorporate bid price of this exchange
            if ( ret.bid.isZeroOrLess() || // bid doesn't exist yet
                 ret.bid > info.ticker.bid ) // bid is higher than the exchange bid
                ret.bid = info.ticker.bid;

            // incorporate ask price of this exchange
            if ( ret.ask.isZeroOrLess() || // ask doesn't exist yet
                 ret.ask < info.ticker.ask ) // ask is lower than the exchange ask
                ret.ask = info.ticker.ask;
        }
    }

    if ( prices_uses_avg )
    {
        // on 0 samples, return here
        if ( samples < 1 )
            return TickerInfo();

        // divide by num of samples if necessary
        if ( samples > 1 )
        {
            ret.bid /= samples;
            ret.ask /= samples;
        }
    }
    else if ( !ret.isValid() )
        return TickerInfo();

    const Coin midprice = ret.getMidPrice();
    ret.bid = midprice;
    ret.ask = midprice;

    return ret;
}

TickerInfo SpruceOverseer::getSpreadForSide( const QString &market, quint8 side, bool order_duplicity, bool taker_mode, bool include_limit_for_side, bool is_randomized, Coin greed_reduce )
{
    if ( side == SIDE_SELL )
        greed_reduce = -greed_reduce;

    /// step 1: get combined spread between all exchanges
    TickerInfo ret = getMidSpread( market );

    /// step 2: apply base greed value to spread
    // get price ticksize
    const Coin ticksize_minimum = getPriceTicksizeForMarket( market );
    Coin ticksize = ticksize_minimum;

    adjustTicksizeToSpread( ticksize, ret, ticksize_minimum );

    /// step 3: adjust spread by distance chosen
    // ensure the spread is more profitable than base greed value
    Coin spread_distance_base = spruce->getOrderGreed();
    const Coin limit = std::min( spread_distance_base + greed_reduce, spruce->getOrderGreedMinimum() );

    // contract our spread in the direction specified
    if ( greed_reduce.isGreaterThanZero() )
        adjustSpread( ret, limit, expand_spread_base_down ? SIDE_BUY : side, ticksize, false );

    // expand further in the direction specified, if needed
    adjustSpread( ret, limit, expand_spread_base_down ? SIDE_BUY : side, ticksize, true );

    // if we included randomness, expand again
    if ( include_limit_for_side )
    {
        Coin spread_distance_random;
        if ( is_randomized )
            spread_distance_random = spruce->getOrderGreedRandom( side );
        else
            spread_distance_random = ( side == SIDE_BUY ) ? spruce->getOrderGreed() - spruce->getOrderRandomBuy() :
                                                            spruce->getOrderGreed() - spruce->getOrderRandomSell();

        const Coin limit_random = std::min( spread_distance_random + greed_reduce, spruce->getOrderGreedMinimum() );

        // expand to limit_random
        adjustSpread( ret, limit_random, side, ticksize, true );
    }

    // if duplicity is on, leave everything normal
    if ( order_duplicity )
    {
        // if taker-mode is also on, run divide-conquer on reversed spread, since we are bidding at
        // the ask price, etc.
        if ( taker_mode )
        {
            const Coin tmp = ret.ask;
            ret.ask = ret.bid;
            ret.bid = tmp;
        }
    }
    else // if order duplicity is off, run divide-conquer on the price at the center of the spread
    {
        const Coin midprice = ret.getMidPrice();
        ret.bid = midprice;
        ret.ask = midprice;
    }

    return ret;
}

Coin SpruceOverseer::getPriceTicksizeForMarket( const Market &market ) const
{
    for ( QMap<quint8, Engine*>::const_iterator i = engine_map.begin(); i != engine_map.end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->market_info.contains( market ) )
            continue;

        const MarketInfo &info = engine->getMarketInfo( market );

        return std::max( info.matcher_ticksize,
                         Coin( info.ticker.bid / 20000 ).truncatedByTicksize( info.matcher_ticksize ) );
    }

    return CoinAmount::SATOSHI;
}

void SpruceOverseer::loadSettings()
{
    const QString path = getSettingsPath();
    QFile loadfile( path );

    if ( !loadfile.open( QIODevice::ReadOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't load spruce settings file" << path;
        return;
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    // emit new lines
    QString data = loadfile.readAll();
    kDebug() << "[SpruceOverseer] loaded spruce settings," << data.size() << "bytes.";

    emit gotUserCommandChunk( data );
}

void SpruceOverseer::saveSettings()
{
    // open settings file
    const QString path = getSettingsPath();

    QFile savefile( path );

    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't open spruce settings file" << path;
        return;
    }

    QTextStream out_savefile( &savefile );

    // save spruce state
    out_savefile << spruce->getSaveState();

    // save the buffer
    out_savefile.flush();
    savefile.close();
}

void SpruceOverseer::loadStats()
{
    QString path = Global::getMarketStatsPath();
    QFile loadfile( path );

    if ( !loadfile.open( QIODevice::ReadWrite | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't load stats file" << path;
        return;
    }

    if ( loadfile.bytesAvailable() == 0 )
        return;

    QString data = loadfile.readAll();
    kDebug() << "[SpruceOverseer] loaded stats," << data.size() << "bytes.";

    alpha->reset();
    alpha->readSaveState( data );
}

void SpruceOverseer::saveStats()
{
    // open stats file
    QString path = Global::getMarketStatsPath();
    QFile savefile( path );

    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        kDebug() << "local error: couldn't open savemarket file" << path;
        return;
    }

    QTextStream out_savefile( &savefile );
    out_savefile << alpha->getSaveState();

    // save the buffer
    out_savefile.flush();
    savefile.close();
}

void SpruceOverseer::onSaveSpruceSettings()
{
    if ( !spruce->isActive() )
        return;

    const QString trader_path = Global::getTraderPath();
    const QString settings_path = trader_path + QDir::separator() + "spruce.settings";
    const QString stats_path = trader_path + QDir::separator() + "stats";

    // backup settings file
    if ( QFile::exists( settings_path ) )
    {
        QString new_settings_path = Global::getOldLogsPath() + QDir::separator() + "spruce.settings." + QString::number( QDateTime::currentSecsSinceEpoch() );
        kDebug() << "backing up spruce settings...";

        if ( QFile::copy( settings_path, new_settings_path ) )
        {
            QFile::remove( settings_path ); // remove old file on success
            saveSettings();
        }
        else
        {
            kDebug() << "local error: couldn't backup settings file to" << new_settings_path;
        }
    }

    // backup stats file
    if ( QFile::exists( stats_path ) )
    {
        QString new_stats_path = Global::getOldLogsPath() + QDir::separator() + "stats." + QString::number( QDateTime::currentSecsSinceEpoch() );
        kDebug() << "backing up spruce stats...";

        if ( QFile::copy( stats_path, new_stats_path ) )
        {
            QFile::remove( stats_path ); // remove old file on success
            saveStats();
        }
        else
        {
            kDebug() << "local error: couldn't backup spruce stats file to" << new_stats_path;
        }
    }
}

void SpruceOverseer::runCancellors( Engine *engine, const QString &market, const quint8 side, const QString &phase_name, const Coin &flux_price )
{
    // sort active positions by longest active first, shortest active last
    const QVector<Position*> active_by_set_time = engine->positions->activeBySetTime();

    // look for spruce positions we should cancel on this side
    const QVector<Position*>::const_iterator begin = active_by_set_time.begin(),
                                             end = active_by_set_time.end();
    for ( QVector<Position*>::const_iterator j = begin; j != end; j++ )
    {
        Position *const &pos = *j;
        bool is_inverse = false;

        // don't skip inverse markets matching this side (the inverse side)
        if (  pos->side != side &&
             !pos->is_cancelling &&
              pos->strategy_tag == phase_name &&
              pos->market.getInverse() == market )
        {
            //kDebug() << "found inverse market for cancellor for pos" << pos->stringifyOrder();
            is_inverse = true;
        }
        // skip non-qualifying position
        else if ( pos->side != side ||
                  pos->is_cancelling ||
                  pos->market != market ||
                  pos->strategy_tag != phase_name )
        {
            continue;
        }

        // get possible spread price vibration limits for new spruce order on this side
        const TickerInfo spread_limit = getSpreadLimit( market, true );

        // set sp1 price
        Coin buy_price_limit, sell_price_limit;
        if ( phase_name.contains( MIDSPREAD_PHASE ) )
        {
            const TickerInfo mid_spread = getMidSpread( market );
            buy_price_limit = mid_spread.bid * Coin("0.99");
            sell_price_limit = mid_spread.ask * Coin("1.01");
        }
        else
        {
            buy_price_limit = spread_limit.bid * Coin("0.999");
            sell_price_limit = spread_limit.ask * Coin("1.001");
        }

        // cache actual side/price
        const quint8 side_actual = is_inverse ? ( ( side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY ) : side;
        const Coin price_actual = is_inverse ? ( CoinAmount::COIN / pos->price ) : pos->price;

        /// cancellor 1: look for prices that are trailing the spread too far
        if ( buy_price_limit.isGreaterThanZero() && sell_price_limit.isGreaterThanZero() && // ticker is valid
             ( ( side_actual == SIDE_BUY  && price_actual < buy_price_limit ) ||
               ( side_actual == SIDE_SELL && price_actual > sell_price_limit ) ) )
        {
            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE );
            continue;
        }

        // skip orders outside flux price
        if ( flux_price.isGreaterThanZero() )
        {
            const Coin flux_price_actual = is_inverse ? ( CoinAmount::COIN / flux_price ) : flux_price;

            // for cancellor 2, only try to cancel positions within the flux bounds
            if ( ( side_actual == SIDE_BUY  && price_actual < flux_price_actual ) ||
                 ( side_actual == SIDE_SELL && price_actual > flux_price_actual ) )
                continue;
        }

        const QString exchange_market_key = QString( "%1-%2" )
                                            .arg( engine->engine_type )
                                            .arg( market );

        // get market allocation
        const Coin active_amount = engine->positions->getActiveSpruceEquityTotal( market, phase_name, side_actual, flux_price );
        const Coin amount_to_shortlong = spruce->getExchangeAllocation( exchange_market_key ) * spruce->getCurrencyPriceByMarket( market ) * spruce->getQuantityToShortLongNow( market );

        // get active tolerance
        const bool is_midspread_phase = phase_name.contains( MIDSPREAD_PHASE );
        const Coin nice_zero_bound = spruce->getOrderNiceZeroBound( market, side_actual, is_midspread_phase );
        const Coin zero_bound_tolerance = spruce->getOrderSize( market ) * nice_zero_bound;

        /// cancellor 2: look for active amount > amount_to_shortlong + order_size_limit
        if ( ( side_actual == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
               active_amount - zero_bound_tolerance > amount_to_shortlong.abs() ) ||
             ( side_actual == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
               active_amount - zero_bound_tolerance > amount_to_shortlong.abs() ) )
        {
            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_2 );
            continue;
        }
    }
}

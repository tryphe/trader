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

const bool prices_uses_avg = true; // false = use widest spread edges, true = average spreads together

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
    autosave_timer->start( 60000 * 60 ); // set default to 1hr, changes when we call setspruceinterval
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

    // agitate
    spruce->runAgitator();

    QMap<QString/*market*/,Coin> spread_price;
    const QList<QString> currencies = spruce->getCurrencies();
    const QList<QString> markets = spruce->getMarketsAlpha();

    for ( QList<QString>::const_iterator m = markets.begin(); m != markets.end(); m++ )
    {
        const Market market_to_trade = *m;

        // one pass each for buys and sells
        for ( quint8 side = SIDE_BUY; side < SIDE_SELL +1; side++ )
        {
            const QString strategy = QString( "spruce-%1-%2" )
                                      .arg( side == SIDE_BUY ? "B" : "S" )
                                      .arg( market_to_trade );

            TickerInfo spread_duplicity = getSpreadForSide( market_to_trade, side, true, false, true, true );

            spruce->clearLiveNodes();
            Coin price_to_use;
            for ( QList<QString>::const_iterator i = currencies.begin(); i != currencies.end(); i++ )
            {
                const QString &currency = *i;
                const Market market( spruce->getBaseCurrency(), currency );
                const TickerInfo mid_spread = getSpreadLimit( market, false );
                price_to_use = ( side == SIDE_BUY ) ? mid_spread.bid_price :
                                                      mid_spread.ask_price;

                // these prices should be equal
                if ( mid_spread.bid_price != mid_spread.ask_price )
                {
                    kDebug() << "local error: midspread bid" << mid_spread.bid_price << "!= ask" << mid_spread.ask_price;
                    return;
                }

                // if the ticker isn't updated, just skip this whole function
                if ( price_to_use.isZeroOrLess() )
                {
                    kDebug() << "[Spruce] local error: no ticker for currency" << market;
                    return;
                }

                // adjust all other prices by surface skew
                price_to_use *= spruce->getSkew();

                // if market matches selected market, select best price from duplicity price or mid price
                if ( market == market_to_trade )
                {
                    // set the most optimistic price to use, either the midprice or duplicity price
                    price_to_use = ( side == SIDE_BUY ) ? std::min( price_to_use, spread_duplicity.bid_price ) :
                                                          std::max( price_to_use, spread_duplicity.ask_price );
                }

                spread_price.insert( currency, price_to_use );
                spruce->addLiveNode( currency, price_to_use );
            }

            // calculate amount to short/long, and fail if necessary
            if ( !spruce->calculateAmountToShortLong() )
                return;

            const QMap<QString,Coin> &qty_to_shortlong_map = spruce->getQuantityToShortLongMap();

            for ( QMap<quint8, Engine*>::const_iterator e = engine_map.begin(); e != engine_map.end(); e++ )
            {
                Engine *engine = e.value();

                for ( QMap<QString,Coin>::const_iterator i = qty_to_shortlong_map.begin(); i != qty_to_shortlong_map.end(); i++ )
                {
                    const QString &market = i.key();

                    // skip market unless it's selected
                    if ( market != market_to_trade )
                        continue;

                    const QString exchange_market_key = QString( "%1-%2" )
                                                        .arg( e.key() )
                                                        .arg( market );

                    // get market allocation for this exchange and apply to qty_to_shortlong
                    const Coin market_allocation = spruce->getExchangeAllocation( exchange_market_key );

                    // continue on zero market allocation for this engine
                    if ( market_allocation.isZeroOrLess() )
                        continue;

                    const Coin qty_to_shortlong = i.value() * market_allocation;
                    const bool is_buy = qty_to_shortlong.isZeroOrLess();

                    // don't place buys during the ask price loop, or sells during the bid price loop
                    if ( (  is_buy && side == SIDE_SELL ) ||
                         ( !is_buy && side == SIDE_BUY ) )
                        continue;

                    const Coin qty_to_shortlong_abs = qty_to_shortlong.abs();
                    const Coin amount_to_shortlong = spruce->getCurrencyPriceByMarket( market ) * qty_to_shortlong;
                    const Coin amount_to_shortlong_abs = amount_to_shortlong.abs();
                    const Coin spruce_active_for_side = engine->positions->getActiveSpruceEquityTotal( market, side, Coin() );
                    //const Coin spruce_active_for_side_up_to_flux_price = engine->positions->getActiveSpruceEquityTotal( market, side, price_to_use );

                    // cache some order info
                    static const int ORDERSIZE_EXPAND_THRESH = 15;
                    static const int ORDERSIZE_EXPAND_MAX = 6;
                    const Coin order_size_unscaled = spruce->getOrderSize( market );
                    const Coin order_size = std::min( order_size_unscaled * ORDERSIZE_EXPAND_MAX, std::max( order_size_unscaled, amount_to_shortlong_abs / ORDERSIZE_EXPAND_THRESH ) );
                    //const Coin order_max = is_buy ? spruce->getMarketBuyMax( market ) :
                    //                                spruce->getMarketSellMax( market );
                    const Coin order_size_limit = order_size_unscaled * spruce->getOrderNice( side );

                    QString order_type = "onetime";
                    Coin &buy_price = spread_duplicity.bid_price;
                    Coin &sell_price = spread_duplicity.ask_price;

                    const Coin spread_put_threshold = order_size_unscaled * spruce->getOrderNiceSpreadPut();

                    // declare spread reduce here so we can print/evalulate it after
                    Coin spread_reduce;

                    // put prices at spread if pending amount to shortlong is greater than size * order_nice_spreadput
                    if ( spread_put_threshold.isGreaterThanZero() &&
                         amount_to_shortlong_abs > spread_put_threshold )
                    {
                        const Coin taker_threshold = order_size_unscaled * spruce->getOrderNiceSpreadPutTaker();

                        // apply taker threshold, amount should be larger than contraction threshold
                        if ( amount_to_shortlong_abs >= taker_threshold )
                        {
                            const TickerInfo taker_spread = getSpreadForSide( market, side, true, true );
                            buy_price = taker_spread.bid_price;
                            sell_price = taker_spread.ask_price;

                            // the bid price should be greater than the ask (because the spread is flipped)
                            if ( taker_spread.bid_price <= taker_spread.ask_price )
                                kDebug() << "local error: taker bid" << taker_spread.bid_price << "< ask" << taker_spread.ask_price;

                            // set local taker mode to disable spread collision detection
                            order_type += "-taker";

                            // set fast timeout
                            order_type += "-timeout5";
                        }
                        // apply soft threshold by contracting the spread
                        else
                        {
                            // amount to reduce greed by = ( amount_to_shortlong / spreadput ) * 0.01
                            // reduce greed by 0.1% for every spread_put_threshold amount we should set
                            spread_reduce = ( amount_to_shortlong_abs / spread_put_threshold ) * ( CoinAmount::SATOSHI * 100000 );

                            const TickerInfo collapsed_spread = getSpreadForSide( market, side, true, false, true, true, spread_reduce );
                            buy_price = collapsed_spread.bid_price;
                            sell_price = collapsed_spread.ask_price;

                            // set slow timeout
                            order_type += QString( "-timeout%1" )
                                          .arg( Global::getSecureRandomRange32( 120, 200 ) );
                        }
                    }

                    // run cancellors for this strategy tag right before we read the active amount
                    // (run down here to avoid running unnecessarily)
                    runCancellors( engine, market_to_trade, side, strategy, side == SIDE_BUY ? buy_price : sell_price );

                    /// detect conflicting positions for this market within the spread distance limit
                    // cache spread distance limit for this side
                    if ( is_buy )
                        last_spread_reduce_buys.insert( market, spread_reduce );
                    else
                        last_spread_reduce_sells.insert( market, spread_reduce );

                    // if we're under the nice size limit, skip conflict checks and order setting
                    if ( amount_to_shortlong_abs < order_size_limit )
                        continue;

                    // cache spread distance limit for this side, but selected larger spread_reduce value from both sides
                    const Coin spread_reduce_selected = std::max( last_spread_reduce_buys.value( market ), last_spread_reduce_sells.value( market ) );
                    const Coin spread_distance_limit = std::min( spruce->getOrderGreed() + spread_reduce_selected, spruce->getOrderGreedMinimum() );

                    // search positions for conflicts
                    for ( QSet<Position*>::const_iterator j = engine->positions->all().begin(); j != engine->positions->all().end(); j++ )
                    {
                        Position *const &pos = *j;

                        // look for positions on the other side of this market
                        if ( pos->side == side ||
                             pos->is_cancelling ||
                             pos->order_set_time == 0 ||
                             pos->market != market ||
                            !pos->strategy_tag.startsWith( "spruce" ) )
                            continue;

                        if ( (  is_buy && buy_price >= pos->sell_price * spread_distance_limit ) ||
                             ( !is_buy && sell_price * spread_distance_limit <= pos->buy_price ) )
                        {
                            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_CONFLICT );
                        }
                    }

                    // don't go over our per-market max
//                    if ( spruce_active_for_side + order_size >= order_max )
//                    {
//                        kDebug() << "[Spruce] info:" << market << "over market" << QString( "%1" ).arg( is_buy ? "buy" : "sell" ) << "order max";
//                        continue;
//                    }

                    // don't go over the abs value of our new projected position
                    // TODO: once we use smoothing for sp3 calculation, use spruce_active_for_side_up_to_flux_price
                    if ( spruce_active_for_side + order_size_limit >= amount_to_shortlong_abs )
                        continue;

                    kDebug() << QString( "[%1] %2 | coeff %3 | qty %4 | amt %5 | active-total %6 | sp_limit %7" )
                                   .arg( strategy, -MARKET_STRING_WIDTH - 9 )
                                   .arg( side == SIDE_BUY ? buy_price : sell_price )
                                   .arg( spruce->getLastCoeffForMarket( market ), 12 )
                                   .arg( qty_to_shortlong, 20 )
                                   .arg( amount_to_shortlong, 13 )
                                   .arg( spruce_active_for_side, 12 )
                                   .arg( order_type.contains( "taker" ) ? "*taker" : spread_distance_limit.toString( 4 ) );

                    // queue the order if we aren't paper trading
#if !defined(PAPER_TRADE)
                    engine->addPosition( market, is_buy ? SIDE_BUY : SIDE_SELL, buy_price, sell_price, order_size,
                                         order_type, strategy, QVector<qint32>(), false, true );
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

        const Coin diff = std::max( spread.bid_price, spread.ask_price ) - std::min( spread.bid_price, spread.ask_price );
        diff_threshold = diff * CONTRACT_RATIO;
    }

    quint32 j = ( side == SIDE_BUY ) ? 0 : 1;
    quint32 debug_reset_count = 0;

    while ( expand ? spread.bid_price > spread.ask_price * limit :
                     spread.bid_price < spread.ask_price * limit )
    {
        // if the side is buy, expand down, otherwise expand outwards
        j++;
        if ( ( side == SIDE_BUY && expand_spread_buys && j % 4 < 3 ) || // if expanding down, only expand down 60% of the time
             ( j % 2 == 1 ) ) // if not expanding down, expand 50/50
            spread.bid_price -= ticksize;
        else
            spread.ask_price += ticksize;

        // only collapse the spread by up to half the distance
        if ( !expand )
        {
            moved += ticksize;
            if ( moved >= diff_threshold )
                break;
        }

        // TODO: NOTE: if the trader doesn't freeze, this prevents an almost infinite loop
        if ( j > 2000 )
        {
            ticksize *= Coin("1.5");
            j = 0;

            if ( ++debug_reset_count > 50 )
                kDebug() << "local warning: increased ticksize 50 times for spread" << spread;
        }
    }
}

void SpruceOverseer::adjustTicksizeToSpread( Coin &ticksize, TickerInfo &spread, const Coin &ticksize_minimum )
{
    const Coin diff = std::max( spread.bid_price, spread.ask_price ) - std::min( spread.bid_price, spread.ask_price );
    if ( diff > ticksize * 1000 )
        ticksize = std::max( diff / 1000, ticksize_minimum );
}

TickerInfo SpruceOverseer::getSpreadLimit( const QString &market, bool order_duplicity )
{
    // get trailing limit for this side
    const Coin trailing_limit_buy = spruce->getOrderTrailingLimit( SIDE_BUY );
    const Coin trailing_limit_sell = spruce->getOrderTrailingLimit( SIDE_SELL );

    // get price ticksize
    // TODO: fix this by reading price matcher ticksize from exchange
    const Coin ticksize_minimum = getPriceTicksizeForMarket( market );
    Coin ticksize = ticksize_minimum;

    // read combined spread from all exchanges. include limts for side, but don't randomize
    TickerInfo ticker_buy = getSpreadForSide( market, SIDE_BUY, order_duplicity, false, true );
    TickerInfo ticker_sell = getSpreadForSide( market, SIDE_SELL, order_duplicity, false, true );

    // first, vibrate one way
    TickerInfo spread1 = TickerInfo( ticker_buy.bid_price, ticker_buy.ask_price );

    // adjust ticksize
    adjustTicksizeToSpread( ticksize, ticker_buy, ticksize_minimum );

    // expand spread
    adjustSpread( spread1, trailing_limit_buy, SIDE_BUY, ticksize );

    // vibrate the other way
    TickerInfo spread2 = TickerInfo( ticker_sell.bid_price, ticker_sell.ask_price );

    // adjust ticksize
    adjustTicksizeToSpread( ticksize, ticker_buy, ticksize_minimum );

    // expand spread
    adjustSpread( spread2, trailing_limit_sell, SIDE_SELL, ticksize );

    // combine vibrations
    TickerInfo combined_spread = TickerInfo( spread1.bid_price, spread2.ask_price );

    if ( !order_duplicity )
    {
        Coin midprice = ( combined_spread.ask_price + combined_spread.bid_price ) / 2;
        combined_spread.bid_price = midprice;
        combined_spread.ask_price = midprice;
    }

    //kDebug() << "spread for" << market << ":" << combined_spread;

    return combined_spread;
}

TickerInfo SpruceOverseer::getSpreadForSide( const QString &market, quint8 side, bool order_duplicity, bool taker_mode, bool include_limit_for_side, bool is_randomized, Coin greed_reduce )
{
    if ( side == SIDE_SELL )
        greed_reduce = -greed_reduce;

    /// step 1: get combined spread between all exchanges
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
            continue;

        const MarketInfo &info = engine->market_info[ market ];

        // ensure prices are valid
        if ( info.highest_buy.isZeroOrLess() || info.lowest_sell.isZeroOrLess() )
            continue;

        // use avg spread
        if ( prices_uses_avg )
        {
            samples++;

            // incorporate prices of this exchange
            ret.bid_price += info.highest_buy;
            ret.ask_price += info.lowest_sell;
        }
        // or, use combined spread edges
        else
        {
            // incorporate bid price of this exchange
            if ( ret.bid_price.isZeroOrLess() || // bid doesn't exist yet
                 ret.bid_price > info.highest_buy ) // bid is higher than the exchange bid
                ret.bid_price = info.highest_buy;

            // incorporate ask price of this exchange
            if ( ret.ask_price.isZeroOrLess() || // ask doesn't exist yet
                 ret.ask_price < info.lowest_sell ) // ask is lower than the exchange ask
                ret.ask_price = info.lowest_sell;
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
            ret.bid_price /= samples;
            ret.ask_price /= samples;
        }
    }
    else if ( ret.bid_price.isZeroOrLess() || ret.bid_price.isZeroOrLess() )
        return TickerInfo();

    /// step 2: apply base greed value to spread
    // get price ticksize
    // TODO: fix this by reading price matcher ticksize from exchange
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
            Coin tmp = ret.ask_price;
            ret.ask_price = ret.bid_price;
            ret.bid_price = tmp;
        }
    }
    else // if order duplicity is off, run divide-conquer on the price at the center of the spread
    {
        Coin midprice = ( ret.ask_price + ret.bid_price ) / 2;
        ret.bid_price = midprice;
        ret.ask_price = midprice;
    }

    return ret;
}

Coin SpruceOverseer::getPriceTicksizeForMarket( const Market &market )
{
    for ( QMap<quint8, Engine*>::const_iterator i = engine_map.begin(); i != engine_map.end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->market_info.contains( market ) )
            continue;

        const MarketInfo &info = engine->getMarketInfo( market );

        return std::max( CoinAmount::SATOSHI, info.highest_buy / 100000 );
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

void SpruceOverseer::runCancellors( Engine *engine, const QString &market, const quint8 side, const QString &strategy, const Coin &flux_price )
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
        quint8 this_pos_side = side;

        // don't skip inverse markets matching this side (the inverse side)
        if (  side != pos->side &&
             !pos->is_cancelling &&
              pos->strategy_tag.startsWith( "spruce" ) &&
              pos->market.getInverse() == market )
        {
            //kDebug() << "found inverse market for cancellor for pos" << pos->stringifyOrder();
            is_inverse = true;
            this_pos_side = ( side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY; // flip side
        }
        // skip non-qualifying position
        else if ( side != pos->side ||
                  pos->is_cancelling ||
                  market != pos->market ||
                  strategy != pos->strategy_tag )
        {
            continue;
        }

        // get possible spread price vibration limits for new spruce order on this side
        const TickerInfo spread_limit = getSpreadLimit( market, true );
        const Coin &buy_price_limit = spread_limit.bid_price;
        const Coin &sell_price_limit = spread_limit.ask_price;

        const Coin price_actual = is_inverse ? ( CoinAmount::COIN / pos->price ) :
                                               pos->price;

        // TODO: test cancellors for inverse markets

        /// cancellor 1: look for prices that are trailing the spread too far
        if ( buy_price_limit.isGreaterThanZero() && sell_price_limit.isGreaterThanZero() && // ticker is valid
             ( ( this_pos_side == SIDE_BUY  && price_actual < buy_price_limit * Coin( "0.99" ) ) ||
               ( this_pos_side == SIDE_SELL && price_actual > sell_price_limit * Coin( "1.01" ) ) ) )
        {
            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE );
            continue;
        }

        const Coin order_max = this_pos_side == SIDE_BUY ? spruce->getMarketBuyMax( market ) :
                                                           spruce->getMarketSellMax( market );

        const Coin active_amount = engine->positions->getActiveSpruceEquityTotal( market, this_pos_side, flux_price );

        /// cancellor 4: look for active amount > order_max
        /// this won't go off normally, only if we change the limit. then this will shave off some orders.
        if ( active_amount > order_max )
        {
            // cancel a random order on that side
            Position *const &pos_to_cancel = spruce->getOrderCancelMode() ? engine->positions->getRandomSprucePosition( market, this_pos_side ) :
                                             this_pos_side == SIDE_BUY ? engine->positions->getHighestSpruceBuy( market ) :
                                                                         engine->positions->getLowestSpruceSell( market );

            // check badptr just incase, but should be impossible to get here
            if ( pos_to_cancel == nullptr )
                return;

            engine->positions->cancel( pos_to_cancel, false, CANCELLING_FOR_SPRUCE_4 );
            continue;
        }

        // for cancellor 3, only try to cancel positions within the flux bounds
        if ( ( this_pos_side == SIDE_BUY  && price_actual < flux_price ) ||
             ( this_pos_side == SIDE_SELL && price_actual > flux_price ) )
            continue;

        const QString exchange_market_key = QString( "%1-%2" )
                                            .arg( engine->engine_type )
                                            .arg( market );

        // get market allocation for this exchange and apply to qty_to_shortlong
        const Coin market_allocation = spruce->getExchangeAllocation( exchange_market_key );
        const Coin amount_to_shortlong = spruce->getCurrencyPriceByMarket( market ) * spruce->getQuantityToShortLongNow( market )
                                         * market_allocation;

        const Coin order_size = spruce->getOrderSize( market );
        const Coin zero_bound_tolerance = order_size * spruce->getOrderNiceZeroBound( side );

        /// cancellor 3: look for active amount > amount_to_shortlong + order_size_limit
        if ( ( this_pos_side == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
                active_amount - zero_bound_tolerance > amount_to_shortlong.abs() ) ||
             ( this_pos_side == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
                active_amount - zero_bound_tolerance > amount_to_shortlong.abs() ) )
        {
            engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_3 );
            continue;
        }
    }
}

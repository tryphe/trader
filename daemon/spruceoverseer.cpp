#include "spruceoverseer.h"
#include "spruce.h"
#include "alphatracker.h"
#include "position.h"
#include "market.h"
#include "engine.h"
#include "positionman.h"

#include <QTimer>
#include <QMap>
#include <QList>
#include <QSet>
#include <QPair>

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
    if ( !spruce->isActive() )
        return;

    // agitate
    spruce->runAgitator();

    QMap<QString/*market*/,Coin> spread_price;
    const QList<QString> currencies = spruce->getCurrencies();
    const QList<QString> markets = spruce->getMarkets();

    for ( QList<QString>::const_iterator m = markets.begin(); m != markets.end(); m++ )
    {
        const Market market_to_trade = *m;

        // one pass each for buys and sells
        for ( quint8 side = SIDE_BUY; side < SIDE_SELL +1; side++ )
        {
            const QString strategy = QString( "spruce-%1-%2" )
                                      .arg( side == SIDE_BUY ? "B" : "S" )
                                      .arg( market_to_trade );

            const TickerInfo duplicity_spread = getSpruceSpreadLimit( market_to_trade, side, true, false );
            const Coin &duplicity_price = ( side == SIDE_BUY ) ? duplicity_spread.bid_price :
                                                                 duplicity_spread.ask_price;

            spruce->clearLiveNodes();
            for ( QList<QString>::const_iterator i = currencies.begin(); i != currencies.end(); i++ )
            {
                const QString &currency = *i;
                const Market market( spruce->getBaseCurrency(), currency );
                const TickerInfo mid_spread = getSpruceSpread( market, nullptr, false, false, false );
                Coin price = ( side == SIDE_BUY ) ? mid_spread.bid_price :
                                                    mid_spread.ask_price;

                // these prices should be equal
                if ( mid_spread.bid_price != mid_spread.ask_price )
                    kDebug() << "local error: midspread bid" << mid_spread.bid_price << "!= ask" << mid_spread.ask_price;

                // if the ticker isn't updated, just skip this whole function
                if ( price.isZeroOrLess() )
                {
                    kDebug() << "[Spruce] local error: no ticker for currency" << market;
                    return;
                }

                if ( (QString) market == (QString) market_to_trade )
                    price = duplicity_price;

                spread_price.insert( currency, price );
                spruce->addLiveNode( currency, price );
            }

            // calculate amount to short/long, and fail if necessary
            if ( !spruce->calculateAmountToShortLong() )
                return;

            // run cancellors for this strategy tag
            runCancellors( market_to_trade, side, strategy );

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
                    const Coin spruce_active_for_side = engine->positions->getActiveSpruceEquityTotal( market, side );

                    // cache some order info
                    static const int ORDERSIZE_EXPAND_THRESH = 16;
                    static const int ORDERSIZE_EXPAND_MAX = 5;
                    const Coin order_size_unscaled = spruce->getOrderSize( market );
                    const Coin order_size = std::min( order_size_unscaled * ORDERSIZE_EXPAND_MAX, std::max( order_size_unscaled, amount_to_shortlong_abs / ORDERSIZE_EXPAND_THRESH ) );
                    const Coin order_max = is_buy ? spruce->getMarketBuyMax( market ) :
                                                    spruce->getMarketSellMax( market );
                    const Coin order_size_limit = order_size_unscaled * spruce->getOrderNice();

                    // get spread price for new spruce order(don't cache because the function generates a random number)
                    TickerInfo spread = getSpruceSpreadForSide( market, side, false );
                    QString order_type = "onetime";

                    // put prices at spread if pending amount to shortlong is greater than size * order_nice_spreadput
                    if ( amount_to_shortlong_abs > order_size_unscaled * spruce->getOrderNiceSpreadPut() )
                    {
                        // incorporate percentage chance of putting order at spread
                        if ( Global::getSecureRandomRange32( 1, 100 ) <= spruce->getOrderNiceSpreadPutPctChance() )
                        {
                            const TickerInfo collapsed_spread = getSpruceSpread( market, nullptr, true, false, true );
                            spread.bid_price = collapsed_spread.bid_price;
                            spread.ask_price = collapsed_spread.ask_price;

                            order_type += "-timeout15";
                        }
                    }
                    const Coin &buy_price = spread.bid_price;
                    const Coin &sell_price = spread.ask_price;

                    // skip noisy amount
                    if ( amount_to_shortlong_abs < order_size_limit )
                        continue;

                    // don't go over our per-market max
                    if ( spruce_active_for_side + order_size >= order_max )
                    {
                        kDebug() << "[Spruce] info:" << market << "over market" << QString( "%1" ).arg( is_buy ? "buy" : "sell" ) << "order max";
                        continue;
                    }

                    // don't go over the abs value of our new projected position
                    if ( spruce_active_for_side + order_size_limit >= amount_to_shortlong_abs )
                        continue;

                    // cancel conflicting spruce positions
                    for ( QSet<Position*>::const_iterator j = engine->positions->all().begin(); j != engine->positions->all().end(); j++ )
                    {
                        Position *const &pos = *j;

                        if ( pos->is_cancelling ||
                             pos->order_set_time == 0 ||
                             pos->market != market ||
                             pos->strategy_tag == strategy )
                            continue;

                        if ( (  is_buy && pos->side == SIDE_SELL && buy_price  >= pos->sell_price.ratio( 0.996 ) ) ||
                             ( !is_buy && pos->side == SIDE_BUY  && sell_price <= pos->buy_price.ratio( 1.004 ) ) )
                        {
                            kDebug() << "[Spruce] cancelling conflicting spruce order" << pos->order_number;
                            engine->positions->cancel( pos );
                        }
                    }

                    kDebug() << QString( "[%1] %2 | coeff %3 | qty %4 | amt %5 | on-order %6" )
                                   .arg( strategy, -MARKET_STRING_WIDTH - 9 )
                                   .arg( side == SIDE_BUY ? spread.bid_price : spread.ask_price )
                                   .arg( spruce->getLastCoeffForMarket( market ), 12 )
                                   .arg( qty_to_shortlong, 20 )
                                   .arg( amount_to_shortlong, 13 )
                                   .arg( spruce_active_for_side, 12 );

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

TickerInfo SpruceOverseer::getSpruceSpreadLimit( const QString &market, quint8 side , bool order_duplicity, bool taker_mode )
{
    // get trailing limit for this side
    const Coin trailing_limit = spruce->getOrderTrailingLimit( side );

    // get price ticksize
    const Coin ticksize = getPriceTicksizeForMarket( market );

    qint64 j = 0;

    // read combined spread from all exchanges
    const TickerInfo ticker = getSpruceSpread( market, &j, order_duplicity, taker_mode, false );

    // first, vibrate one way
    TickerInfo spread1 = TickerInfo( ticker.bid_price, ticker.ask_price );
    Coin &buy1_price = spread1.bid_price;
    Coin &sell1_price = spread1.ask_price;

    while ( buy1_price > sell1_price * trailing_limit )
    {
        if ( j++ % 2 == 0 )
            buy1_price -= ticksize;
        else
            sell1_price += ticksize;
    }

    // vibrate the other way
    TickerInfo spread2 = TickerInfo( ticker.bid_price, ticker.ask_price );
    Coin &buy2_price = spread2.bid_price;
    Coin &sell2_price = spread2.ask_price;

    j++;
    while ( buy2_price > sell2_price * trailing_limit )
    {
        if ( j++ % 2 == 0 )
            buy2_price -= ticksize;
        else
            sell2_price += ticksize;
    }

    // combine vibrations
    TickerInfo combined_spread = TickerInfo( std::min( buy1_price,  buy2_price ),
                                             std::max( sell1_price, sell2_price ) );

    return combined_spread;
}

TickerInfo SpruceOverseer::getSpruceSpread( const QString &market, qint64 *j_ptr, bool order_duplicity, bool taker_mode, bool spread_collapse )
{
    /// step 1: get combined spread between all exchanges
    TickerInfo ret;

    for ( QMap<quint8, Engine*>::const_iterator i = engine_map.begin(); i != engine_map.end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->market_info.contains( market ) )
            continue;

        const MarketInfo &info = engine->market_info[ market ];

        // ensure prices are valid
        if ( info.highest_buy.isZeroOrLess() || info.lowest_sell.isZeroOrLess() )
            continue;

        // incorporate bid price of this exchange
        if ( ret.bid_price.isZeroOrLess() || // bid doesn't exist yet
             ret.bid_price > info.highest_buy ) // bid is higher than the exchange bid
            ret.bid_price = info.highest_buy;

        // incorporate ask price of this exchange
        if ( ret.ask_price.isZeroOrLess() || // ask doesn't exist yet
             ret.ask_price < info.lowest_sell ) // ask is lower than the exchange ask
            ret.ask_price = info.lowest_sell;
    }

    /// step 2: apply base greed value to spread
    // get price ticksizebool spread_collapse = true,
    Coin ticksize = getPriceTicksizeForMarket( market );

    Coin &buy_price = ret.bid_price;
    Coin &sell_price = ret.ask_price;

    // ensure the spread is more profitable than base greed value
    qint64 j = 0;
    Coin greed = spruce->getOrderGreed();

    // alternate between subtracting from sell side first to buy side first
    const bool greed_vibrate_state = Global::getSecureRandomRange32( 0, 1 ) == 0;

    // apply stepping to ticksize to avoid high cpu loop
    const Coin diff = std::max( buy_price, sell_price ) - std::min( buy_price, sell_price );
    const Coin diffd2 = diff / 2;
    Coin moved;
    if ( diff > ticksize * 1000 )
        ticksize = std::max( diff / 1000, CoinAmount::SATOSHI );

    // collapse our spread by up to half the difference of the exchange spread
    if ( spread_collapse )
    {
        // reduce greed by half if we're collapsing the spread
        const Coin greed_reduce = ( CoinAmount::COIN - greed ) /2;

        if ( greed_reduce.isGreaterThanZero() && greed + greed_reduce < CoinAmount::COIN  )
        {
            //kDebug() << "greed" << greed << "->" << ( greed + greed_reduce );
            greed += greed_reduce;
        }

        while ( buy_price < sell_price * greed )
        {
            if ( j++ % 2 == greed_vibrate_state ? 0 : 1 )
                buy_price += ticksize;
            else
                sell_price -= ticksize;

            // don't adjust spread more than half of the diff
            moved += ticksize;
            if ( moved >= diffd2 )
                break;
        }
    }

    // expand wider if needed
    while ( buy_price > sell_price * greed )
    {
        if ( j++ % 2 == greed_vibrate_state ? 0 : 1 )
            buy_price -= ticksize;
        else
            sell_price += ticksize;
    }

    // copy non-default j into passed pointer value
    if ( j > 0 && j_ptr != nullptr )
        *j_ptr = j;

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

TickerInfo SpruceOverseer::getSpruceSpreadForSide( const QString &market, quint8 side, bool order_duplicity, const bool taker_mode )
{
    // get price ticksize
    const Coin ticksize = getPriceTicksizeForMarket( market );

    // read combined spread from all exchanges
    qint64 j = 0;
    TickerInfo spread = getSpruceSpread( market, &j, order_duplicity, taker_mode, false );

    if ( taker_mode )
        return spread;

    Coin &buy_price = spread.bid_price;
    Coin &sell_price = spread.ask_price;

    // ensure the spread greed is applied for this side
    const Coin greed = spruce->getOrderGreed( side );

    // alternate between subtracting from sell side first to buy side first
    const bool greed_vibrate_state = Global::getSecureRandomRange32( 0, 1 ) == 0;

    while ( buy_price > sell_price * greed )
    {
        if ( j++ % 2 == greed_vibrate_state ? 0 : 1 )
            buy_price -= ticksize;
        else
            sell_price += ticksize;
    }

    return spread;
}

Coin SpruceOverseer::getPriceTicksizeForMarket( const QString &market )
{
    Coin ret = CoinAmount::SATOSHI;

    for ( QMap<quint8, Engine*>::const_iterator i = engine_map.begin(); i != engine_map.end(); i++ )
    {
        Engine *engine = i.value();

        // ensure ticker exists
        if ( !engine->market_info.contains( market ) )
            continue;

        const MarketInfo &info = engine->market_info[ market ];

        if ( info.price_ticksize > ret )
            ret = info.price_ticksize;
    }

    return ret;
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

void SpruceOverseer::runCancellors( const QString &market, const quint8 side, const QString &strategy )
{
    // because price is atomic, incorporate a limit for trailing price cancellor 1 for each market.
    // 1 = cancelling pace matches the pace of setting orders, 2 = double the pace
    static const int CANCELLOR1_LIMIT = 1;
    QMap<QString,int> cancellor1_current;

    for ( QMap<quint8, Engine*>::const_iterator e = engine_map.begin(); e != engine_map.end(); e++ )
    {
        Engine *engine = e.value();

        // sort active positions by longest active first, shortest active last
        const QVector<Position*> active_by_set_time = engine->positions->activeBySetTime();

        // look for spruce positions we should cancel on this side
        const QVector<Position*>::const_iterator begin = active_by_set_time.begin(),
                                                 end = active_by_set_time.end();
        for ( QVector<Position*>::const_iterator j = begin; j != end; j++ )
        {
            Position *const &pos = *j;

            // skip non-qualifying position
            if ( side != pos->side ||
                 pos->is_cancelling ||
                 market != pos->market ||
                 strategy != pos->strategy_tag )
                continue;

            // get possible spread price vibration limits for new spruce order on this side
            const QString &market = pos->market;
            const TickerInfo spread_limit = getSpruceSpreadLimit( market, side, true, false );
            const Coin &buy_price_limit = spread_limit.bid_price;
            const Coin &sell_price_limit = spread_limit.ask_price;

            /// cancellor 1: look for prices that are trailing the spread too far
            if ( buy_price_limit.isGreaterThanZero() && sell_price_limit.isGreaterThanZero() && // ticker is valid
                 ( ( pos->side == SIDE_BUY  && pos->price < buy_price_limit ) ||
                   ( pos->side == SIDE_SELL && pos->price > sell_price_limit ) ) )
            {
                // limit cancellor 1 to trigger CANCELLOR1_LIMIT times, per market, per side, per spruce tick
                if ( ++cancellor1_current[ market ] <= CANCELLOR1_LIMIT )
                {
                    engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE );
                    continue;
                }
            }

            const QString exchange_market_key = QString( "%1-%2" )
                                                .arg( e.key() )
                                                .arg( market );

            // get market allocation for this exchange and apply to qty_to_shortlong
            const Coin market_allocation = spruce->getExchangeAllocation( exchange_market_key );
            const Coin amount_to_shortlong = spruce->getCurrencyPriceByMarket( market ) * spruce->getQuantityToShortLongNow( market )
                                             * market_allocation;

            const Coin order_size = spruce->getOrderSize( market );
            const Coin order_size_limit = order_size * spruce->getOrderNice();
            const Coin zero_bound_tolerance = order_size * spruce->getOrderNiceZeroBound();

            /// cancellor 2: if the order is active but our rating is the opposite polarity, cancel it
            if ( ( pos->side == SIDE_BUY  && amount_to_shortlong >  zero_bound_tolerance ) ||
                 ( pos->side == SIDE_SELL && amount_to_shortlong < -zero_bound_tolerance ) )
            {
                engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_2 );
                continue;
            }

            // store active spruce offset for this side
            const Coin spruce_offset = engine->positions->getActiveSpruceOrdersOffset( market, side );

            /// cancellor 3: look for spruce active <> what we should short/long
            if ( ( pos->side == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
                   amount_to_shortlong + spruce_offset >  order_size_limit + zero_bound_tolerance ) ||
                 ( pos->side == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
                   amount_to_shortlong + spruce_offset < -order_size_limit - zero_bound_tolerance ) )
            {
                // cancel a random order on that side
                Position *const &pos_to_cancel = engine->positions->getRandomSprucePosition( market, side );                                                         engine->positions->getHighestSpruceSell( market );

                // check badptr just incase, but should be impossible to get here
                if ( !pos_to_cancel )
                    continue;

                engine->positions->cancel( pos_to_cancel, false, CANCELLING_FOR_SPRUCE_3 );
                continue;
            }

            const Coin active_amount = engine->positions->getActiveSpruceEquityTotal( market, side );

            /// cancellor 4: look for active amount > amount_to_shortlong + order_size_limit
            if ( ( pos->side == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
                   -active_amount < amount_to_shortlong - order_size_limit - zero_bound_tolerance ) ||
                 ( pos->side == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
                    active_amount > amount_to_shortlong + order_size_limit + zero_bound_tolerance ) )
            {
                engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_4 );
                continue;
            }

            const Coin order_max = side == SIDE_BUY ? spruce->getMarketBuyMax( market ) :
                                                      spruce->getMarketSellMax( market );

            /// cancellor 5: look for active amount > order_max (so we can change the value in realtime)
            if ( active_amount > order_max )
            {
                // cancel a random order on that side
                Position *const &pos_to_cancel = engine->positions->getRandomSprucePosition( market, side );                                                         engine->positions->getHighestSpruceSell( market );

                // check badptr just incase, but should be impossible to get here
                if ( !pos_to_cancel )
                    continue;

                engine->positions->cancel( pos_to_cancel, false, CANCELLING_FOR_SPRUCE_5 );
                continue;
            }
        }
    }
}

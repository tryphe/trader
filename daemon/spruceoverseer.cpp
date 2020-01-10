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
#include <QRandomGenerator>

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
    autosave_timer->start( spruce_timer->interval() *4 );
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
    const QList<QString> &currencies = spruce->getCurrencies();

    const Coin long_max = spruce->getLongMax();
    const Coin short_max = spruce->getShortMax();

    // one pass each for buys and sells
    for ( quint8 side = SIDE_BUY; side < SIDE_SELL +1; side++ )
    {
        spruce->clearLiveNodes();
        for ( QList<QString>::const_iterator i = currencies.begin(); i != currencies.end(); i++ )
        {
            const QString &currency = *i;
            const Market market( spruce->getBaseCurrency(), currency );
            const TickerInfo spread = getSpreadForMarket( market );
            const Coin &price = ( side == SIDE_BUY ) ? spread.bid_price :
                                                       spread.ask_price;

            // if the ticker isn't updated, just skip this whole function
            if ( price.isZeroOrLess() )
            {
                kDebug() << "[Spruce] local error: no ticker for currency" << market;
                return;
            }

            spread_price.insert( currency, price );
            spruce->addLiveNode( currency, price );
        }

        // calculate amount to short/long, and fail if necessary
        if ( !spruce->calculateAmountToShortLong() )
            return;

        const QMap<QString,Coin> &qty_to_shortlong_map = spruce->getQuantityToShortLongMap();

        kDebug() << QString( "[Spruce %1] hi-lo coeffs[%2 %3 %4 %5]" )
                        .arg( side == SIDE_BUY ? "buys " : "sells" )
                        .arg( spruce->startCoeffs().lo_currency )
                        .arg( spruce->startCoeffs().lo_coeff )
                        .arg( spruce->startCoeffs().hi_currency )
                        .arg( spruce->startCoeffs().hi_coeff );

        // run cancellors for all markets, just for this side
        runCancellors( side );

        for ( QMap<quint8, Engine*>::const_iterator e = engine_map.begin(); e != engine_map.end(); e++ )
        {
            Engine *engine = e.value();

            for ( QMap<QString,Coin>::const_iterator i = qty_to_shortlong_map.begin(); i != qty_to_shortlong_map.end(); i++ )
            {
                const QString &market = i.key();
                const QString exchange_market_key = QString( "%1-%2" )
                                                    .arg( e.key() )
                                                    .arg( market );

                // get market allocation for this exchange and apply to qty_to_shortlong
                const Coin market_allocation = spruce->getExchangeAllocation( exchange_market_key );
                const Coin qty_to_shortlong = i.value() * market_allocation;

                const Coin qty_to_shortlong_abs = qty_to_shortlong.abs();
                const Coin amount_to_shortlong = spruce->getCurrencyPriceByMarket( market ) * qty_to_shortlong;
                const Coin amount_to_shortlong_abs = amount_to_shortlong.abs();
                const Coin spruce_active_for_side = engine->positions->getActiveSpruceEquityTotal( market, side );

                // cache some order info
                static const int ORDERSIZE_EXPAND_THRESH = 20;
                static const int ORDERSIZE_EXPAND_MAX = 5;
                const bool is_buy = qty_to_shortlong.isZeroOrLess();
                const Coin order_size_unscaled = spruce->getOrderSize( market );
                const Coin order_size = std::min( order_size_unscaled * ORDERSIZE_EXPAND_MAX, std::max( order_size_unscaled, amount_to_shortlong_abs / ORDERSIZE_EXPAND_THRESH ) );
                const Coin order_max = is_buy ? spruce->getMarketBuyMax( market ) :
                                                spruce->getMarketSellMax( market );
                const Coin order_size_limit = order_size_unscaled * spruce->getOrderNice();

                // get spread price for new spruce order(don't cache because the function generates a random number)
                TickerInfo spread = getSpruceSpread( market, side );

                // put prices at spread if pending amount to shortlong is greater than size * order_nice_spreadput
                if ( amount_to_shortlong_abs - spruce_active_for_side > order_size_unscaled * spruce->getOrderNiceSpreadPut() )
                {
                    const TickerInfo live_spread = getSpreadForMarket( market );
                    spread.bid_price = live_spread.bid_price;
                    spread.ask_price = live_spread.ask_price;
                }
                const Coin &buy_price = spread.bid_price;
                const Coin &sell_price = spread.ask_price;

                // don't place buys during the ask price loop, or sells during the bid price loop
                if ( (  is_buy && side == SIDE_SELL ) ||
                     ( !is_buy && side == SIDE_BUY ) )
                    continue;

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

                // are we too long/short to place another order on this side?
                if ( is_buy && spruce_active_for_side > long_max )
                {
                    kDebug() << "[Spruce] info:" << market << "too long for now";
                    continue;
                }
                else if ( !is_buy && spruce_active_for_side > short_max.abs() )
                {
                    kDebug() << "[Spruce] info:" << market << "too short for now";
                    continue;
                }

                // cancel conflicting spruce positions
                for ( QSet<Position*>::const_iterator j = engine->positions->all().begin(); j != engine->positions->all().end(); j++ )
                {
                    Position *const &pos = *j;

                    if ( !pos->is_spruce ||
                          pos->is_cancelling ||
                          pos->order_set_time == 0 ||
                          pos->market != market )
                        continue;

                    if ( (  is_buy && pos->side == SIDE_SELL && buy_price  >= pos->sell_price.ratio( 0.9945 ) ) ||
                         ( !is_buy && pos->side == SIDE_BUY  && sell_price <= pos->buy_price.ratio( 1.0055 ) ) )
                    {
                        kDebug() << "[Spruce] cancelling conflicting spruce order" << pos->order_number;
                        engine->positions->cancel( pos );
                    }
                }

                kDebug() << QString( "[Spruce %1] %2 @ %3 | coeff %4 | qty %5 | amt %6 | on-order %7" )
                               .arg( side == SIDE_BUY ? "buys " : "sells" )
                               .arg( market, MARKET_STRING_WIDTH )
                               .arg( side == SIDE_BUY ? spread.bid_price : spread.ask_price )
                               .arg( spruce->getLastCoeffForMarket( market ), 12 )
                               .arg( qty_to_shortlong, 20 )
                               .arg( amount_to_shortlong, 13 )
                               .arg( spruce_active_for_side, 12 );

                // queue the order if we aren't paper trading
#if !defined(PAPER_TRADE)
                engine->addPosition( market, is_buy ? SIDE_BUY : SIDE_SELL, buy_price, sell_price, order_size,
                                     "onetime-spruce", "spruce", QVector<qint32>(), false, true );
#endif
            }
        }
    }
}

TickerInfo SpruceOverseer::getSpruceSpread( const QString &market, quint8 side )
{
    // get price ticksize
    const Coin ticksize = getPriceTicksizeForMarket( market );

    // read combined spread from all exchanges
    qint64 j = 0;
    TickerInfo spread = getSpreadForMarket( market, &j );

    if ( spruce->getOrderDuplicity() && spruce->getTakerMode() )
        return spread;

    Coin &buy_price = spread.bid_price;
    Coin &sell_price = spread.ask_price;

    // ensure the spread is more profitable than fee*2

    const Coin greed = spruce->getOrderGreed( side );

    // alternate between subtracting from sell side first to buy side first
    const bool greed_vibrate_state = QRandomGenerator::global()->generate() % 2 == 0;

    while ( buy_price > sell_price * greed )
    {
        if ( j++ % 2 == greed_vibrate_state ? 0 : 1 )
            buy_price -= ticksize;
        else
            sell_price += ticksize;
    }

    return spread;
}

TickerInfo SpruceOverseer::getSpruceSpreadLimit( const QString &market, quint8 side )
{
    // get trailing limit for this side
    const Coin trailing_limit = spruce->getOrderTrailingLimit( side );

    // get price ticksize
    const Coin ticksize = getPriceTicksizeForMarket( market );

    qint64 j = 0;

    // read combined spread from all exchanges
    const TickerInfo ticker = getSpreadForMarket( market, &j );

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

TickerInfo SpruceOverseer::getSpreadForMarket( const QString &market , qint64 *j_ptr )
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
    // get price ticksize
    const Coin ticksize = getPriceTicksizeForMarket( market );

    Coin &buy_price = ret.bid_price;
    Coin &sell_price = ret.ask_price;

    // ensure the spread is more profitable than base greed value
    qint64 j = 0;
    const Coin greed = spruce->getOrderGreed();

    // alternate between subtracting from sell side first to buy side first
    const bool greed_vibrate_state = QRandomGenerator::global()->generate() % 2 == 0;

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
    if ( spruce->getOrderDuplicity() )
    {
        // if taker-mode is also on, run divide-conquer on reversed spread, since we are bidding at
        // the ask price, etc.
        if ( spruce->getTakerMode() )
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

void SpruceOverseer::runCancellors( const quint8 side )
{
    // because price is atomic, incorporate a limit for trailing price cancellor 1 for each market.
    // 1 = cancelling pace matches the pace of setting orders, 2 = double the pace
    static const int CANCELLOR1_LIMIT = 2;
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

            // search for stale spruce order for the side we are setting
            if ( !pos->is_spruce ||
                  pos->is_cancelling ||
                  pos->order_set_time == 0 ||
                  side != pos->side )
                continue;

            // get possible spread price vibration limits for new spruce order on this side
            const QString &market = pos->market;
            const TickerInfo spread_limit = getSpruceSpreadLimit( market, side );
            const Coin &buy_price_limit = spread_limit.bid_price;
            const Coin &sell_price_limit = spread_limit.ask_price;

            /// cancellor 1: look for prices that are trailing the spread too far
            if ( buy_price_limit.isGreaterThanZero() && sell_price_limit.isGreaterThanZero() &&
                 ( pos->price < buy_price_limit || pos->price > sell_price_limit ) )
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

            /// cancellor 2: if the order is active but our rating is the opposite polarity, cancel it
            if ( ( amount_to_shortlong >  order_size * spruce->getOrderNiceZeroBound() && pos->side == SIDE_BUY  ) ||
                 ( amount_to_shortlong < -order_size * spruce->getOrderNiceZeroBound() && pos->side == SIDE_SELL ) )
            {
                engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_2 );
                continue;
            }

            // store active spruce offset for this side
            const Coin spruce_offset = engine->positions->getActiveSpruceOrdersOffset( market, side );

            /// cancellor 3: look for spruce active <> what we should short/long
            if ( ( pos->side == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
                   amount_to_shortlong + spruce_offset >  order_size_limit ) ||
                 ( pos->side == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
                   amount_to_shortlong + spruce_offset < -order_size_limit ) )
            {
                // if it's a buy, we should cancel the lowest price. if it's a sell, cancel the highest price.
                Position *const &pos_to_cancel = pos->side == SIDE_BUY ? engine->positions->getLowestSpruceBuy( market ) :
                                                                         engine->positions->getHighestSpruceSell( market );

                // check badptr just incase, but should be impossible to get here
                if ( !pos_to_cancel )
                    continue;

                engine->positions->cancel( pos_to_cancel, false, CANCELLING_FOR_SPRUCE_3 );
                continue;
            }

            const Coin active_amount = engine->positions->getActiveSpruceEquityTotal( market, side );

            /// cancellor 4: look for active amount > amount_to_shortlong + order_size_limit
            if ( ( pos->side == SIDE_BUY  && amount_to_shortlong.isZeroOrLess() &&
                   -active_amount < amount_to_shortlong - order_size_limit ) ||
                 ( pos->side == SIDE_SELL && amount_to_shortlong.isGreaterThanZero() &&
                    active_amount > amount_to_shortlong + order_size_limit ) )
            {
                engine->positions->cancel( pos, false, CANCELLING_FOR_SPRUCE_4 );
                continue;
            }
        }
    }
}

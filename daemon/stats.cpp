#include "stats.h"
#include "trexrest.h"
#include "bncrest.h"
#include "polorest.h"
#include "positionman.h"
#include "global.h"
#include "position.h"
#include "engine.h"

Stats::Stats( Engine *_engine, REST_OBJECT *_rest )
    : engine( _engine ),
      rest( _rest )
{
}

Stats::~Stats()
{
    // these are deleted in trader
    engine = nullptr;
    rest = nullptr;

    kDebug() << "[Stats] done.";
}

void Stats::updateStats( Position *const &pos )
{
    const QString &market = pos->market;
    alpha.addAlpha( market, pos );

    // stringify date + market
    QString date_str = Global::getDateStringMDY(); // cache mdy
    QString date_market_str = QString( "%1 %2" )
                                .arg( date_str )
                                .arg( market, 8 );

    // update some stats
    daily_market_volume[ date_market_str ] += pos->btc_amount;
    daily_volumes[ date_str ] += pos->btc_amount;
    daily_fills[ date_str ]++;
    last_price[ market ] = pos->price;

    // add shortlong strategy stats (use blank tag for all onetime orders)
    addStrategyStats( pos );
}

void Stats::addStrategyStats( Position *const &pos )
{
    // track stats related to this strategy tag
    Coin amount;
    if ( pos->side == SIDE_BUY )
        amount += pos->btc_amount;
    else
        amount -= pos->btc_amount;

    shortlong[ pos->strategy_tag ][ pos->market ] += amount;

    if ( pos->is_spruce )
        engine->spruce.addToShortLonged( pos->market, amount );
}

void Stats::clearAll()
{
    alpha.reset();

    daily_market_volume.clear();
    daily_volumes.clear();
    last_price.clear();
}

void Stats::printOrders( QString market )
{
    QMultiMap<QString/*price*/, Position*> sorted_orders;

    // sort positions_all into map
    for ( QSet<Position*>::const_iterator i = engine->positions->all().begin(); i != engine->positions->all().end(); i++ )
    {
        Position *pos = *i;

        if ( market == pos->market )
            sorted_orders.insert( pos->price, pos );
    }

    Coin sell_total;
    Coin buy_total;
    Coin hi_buy;
    Coin lo_sell = CoinAmount::A_LOT;

    // print the map
    for ( QMultiMap<QString /*price*/, Position*>::const_iterator i = sorted_orders.begin(); i != sorted_orders.end(); i++ )
    {
        const Position *const &pos = i.value();
        const QString &price = pos->price;

        QString output_str = QString( "%1:  %2%3 %4%5>>>none<<< %6 o %7 %8" )
                .arg( pos->price, -10 )
                .arg( pos->is_landmark ? "L" : " " )
                .arg( pos->is_slippage ? "S" : " " )
                .arg( pos->side == SIDE_BUY ? ">>>grn<<<" : ">>>red<<<" )
                .arg( pos->sideStr(), -4 )
                .arg( pos->btc_amount )
                .arg( pos->order_number, -11 )
                .arg( pos->indices_str, -3 );

        kDebug() << output_str;

        // calculate totals
        if      ( pos->side == SIDE_BUY  ) buy_total += pos->btc_amount;
        else if ( pos->side == SIDE_SELL ) sell_total += pos->btc_amount;

        // calculate hi_buy/lo_sell
        if      ( pos->side == SIDE_BUY  && price > hi_buy  ) hi_buy = price;
        else if ( pos->side == SIDE_SELL && price < lo_sell ) lo_sell = price;
    }

    kDebug() << "buy total:" << buy_total;
    kDebug() << "sell total:" << sell_total;
    kDebug() << "spread gap:" << lo_sell - hi_buy;
}

void Stats::printOrdersByIndex( QString market )
{
    QMultiMap<qint32/*idx*/, Position*> sorted_orders;

    // sort positions_all into sorted_orders
    for ( QSet<Position*>::const_iterator i = engine->positions->all().begin(); i != engine->positions->all().end(); i++ )
    {
        Position *pos = *i;

        if ( market == pos->market )
            sorted_orders.insert( pos->getLowestMarketIndex(), pos );
    }

    Coin sell_total;
    Coin buy_total;
    Coin hi_buy;
    Coin lo_sell = CoinAmount::A_LOT;

    // print the map
    for ( QMultiMap<qint32 /*idx*/, Position*>::const_iterator i = sorted_orders.begin(); i != sorted_orders.end(); i++ )
    {
        const Position *const &pos = i.value();
        const QString &price = pos->price;

        QString output_str = QString( "%1:  %2%3 %4%5>>>none<<< %6 o %7 %8" )
                .arg( pos->price, -10 )
                .arg( pos->is_landmark ? "L" : " " )
                .arg( pos->is_slippage ? "S" : " " )
                .arg( pos->side == SIDE_BUY ? ">>>grn<<<" : ">>>red<<<" )
                .arg( pos->sideStr(), -4 )
                .arg( pos->btc_amount )
                .arg( pos->order_number, -11 )
                .arg( pos->indices_str, -3 );

        kDebug() << output_str;

        // calculate totals
        if      ( pos->side == SIDE_BUY  ) buy_total += pos->btc_amount;
        else if ( pos->side == SIDE_SELL ) sell_total += pos->btc_amount;

        // calculate hi_buy/lo_sell
        if      ( pos->side == SIDE_BUY  && price > hi_buy  ) hi_buy = price;
        else if ( pos->side == SIDE_SELL && price < lo_sell ) lo_sell = price;
    }

    kDebug() << "buy total:" << buy_total;
    kDebug() << "sell total:" << sell_total;
    kDebug() << "spread gap:" << lo_sell - hi_buy;
}

void Stats::printPositions( QString market )
{
    if ( engine->getMarketInfo( market ).position_index.size() < 1 )
        return;

    const QVector<PositionData> &list = engine->getMarketInfo( market ).position_index;

    for ( int i = 0; i < list.size(); i++ )
    {
        const PositionData &data = list.value( i );

        kDebug() << QString( "lo %1 hi %2 size %3 fills %4 alt %5" )
                        .arg( data.buy_price )
                        .arg( data.sell_price )
                        .arg( data.order_size )
                        .arg( data.fill_count, -3 )
                        .arg( data.alternate_size );
    }
}

void Stats::printDailyVolumes()
{
    QMap<QString /*market*/, Coin /*volume*/>::const_iterator i;
    for ( i = daily_volumes.begin(); i != daily_volumes.end(); i++ )
    {
        const QString &market = i.key();
        const Coin &volume = daily_volumes.value( market );

        kDebug() << QString( "%1: %2" )
                .arg( market, 8 )
                .arg( volume );
    }
}

void Stats::printDailyFills()
{
    QMap<QString /*market*/, qint32 /*volume*/>::const_iterator i;
    for ( i = daily_fills.begin(); i != daily_fills.end(); i++ )
    {
        const QString &date = i.key();
        qint32 fills = daily_fills.value( date );

        kDebug() << QString( "%1: %2" )
                .arg( date, 8 )
                .arg( fills );
    }
}

void Stats::printLastPrices()
{
    QMap<QString /*market*/, QString /*price*/>::const_iterator i;
    for ( i = last_price.begin(); i != last_price.end(); i++ )
    {
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( i.value() );
    }
}

void Stats::printBuySellTotal()
{
    QMap<QString /*market*/, qint32> buys, sells, total;

    // build indexes from active and queued positions
    for ( QSet<Position*>::const_iterator i = engine->positions->all().begin(); i != engine->positions->all().end(); i++ )
    {
        Position *const &pos = *i;

        if ( pos->side == SIDE_BUY )
            buys[ pos->market ]++;
        else
            sells[ pos->market ]++;

        // save the total count
        total[ pos->market ]++;
    }

    kDebug() << "buys: " << buys;
    kDebug() << "sells:" << sells;
    kDebug() << "total:" << total;
}

void Stats::printStrategyShortLong( QString strategy_tag )
{
    Coin total;
    kDebug() << "totals for shortlong strategy" << strategy_tag << ":";

    const QMap<QString/*currency*/,Coin/*short-long*/> &mapref = shortlong.value( strategy_tag );
    for ( QMap<QString,Coin>::const_iterator i = mapref.begin(); i != mapref.end(); i++ )
    {
        const Coin &amount = i.value();

        total += amount;
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), -8 )
                .arg( amount );
    }

    kDebug() << "shortlong total:" << total;
}

void Stats::printDailyMarketVolume()
{
    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = daily_market_volume.begin(); i != daily_market_volume.end(); i++ )
    {
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( i.value() );
    }
}

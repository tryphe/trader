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

void Stats::updateStats( const QString &fill_type_str, const QString &market, const QString &order_id, const quint8 side,
                         const QString &strategy_tag, const Coin &btc_amount, const Coin &quantity, const Coin &price,
                         const Coin &btc_commission, bool partial_fill )
{
    Coin final_btc_amount = btc_amount - btc_commission;
    Coin final_quantity = quantity - ( btc_commission / price );

    if ( engine->getVerbosity() > 0 )
    {
        const bool is_buy = ( side == SIDE_BUY );
        const QString side_str = QString( "%1%2>>>none<<<" )
                                 .arg( is_buy ? ">>>grn<<<" : ">>>red<<<" )
                                 .arg( is_buy ? "buy " : "sell" );

        kDebug() << QString( "%1(%2): %3 %4 %5 (c %7 (q %8 @ %9 o %10" )
                    .arg( partial_fill ? "part" : "full" )
                    .arg( fill_type_str, -8 )
                    .arg( side_str )
                    .arg( market, MARKET_STRING_WIDTH )
                    .arg( btc_amount, PRICE_WIDTH )
                    .arg( btc_commission + ")", -PRICE_WIDTH -1 )
                    .arg( final_quantity + ")", -PRICE_WIDTH -1 )
                    .arg( price, -PRICE_WIDTH )
                    .arg( order_id );
    }

    m_alpha.addAlpha( market, side, final_btc_amount, price, partial_fill );

    // stringify date + market
    QString date_str = Global::getDateStringMDY(); // cache mdy
    QString date_market_str = QString( "%1 %2" )
                                .arg( date_str )
                                .arg( market, 8 );

    // update some stats
    daily_market_volume[ date_market_str ] += final_btc_amount;
    daily_volumes[ date_str ] += final_btc_amount;
    last_price[ market ] = price;

    // update fill count
    if ( !partial_fill )
        daily_fills[ date_str ]++;

    // track stats offset related to this strategy
    const Coin amount_offset = ( side == SIDE_BUY ) ?  final_btc_amount
                                                    : -final_btc_amount;

    const Coin quantity_offset = ( side == SIDE_BUY ) ?  final_quantity
                                                      : -final_quantity;

    shortlong[ strategy_tag ][ market ] += amount_offset;

    if ( strategy_tag == "spruce" )
        engine->getSpruce().addToShortLonged( market, quantity_offset );

}

void Stats::clearAll()
{
    m_alpha.reset();

    daily_market_volume.clear();
    daily_volumes.clear();
    last_price.clear();
}

void Stats::printOrders( const QString &market, bool by_index )
{
    QMultiMap<Coin/*price/idx*/,Position*> sorted_orders;

    // sort positions_all into map
    QSet<Position*>::const_iterator begin = engine->getPositionMan()->all().begin(),
                                    end = engine->getPositionMan()->all().end();
    for ( QSet<Position*>::const_iterator i = begin; i != end; i++ )
    {
        Position *pos = *i;

        if ( market == pos->market )
        {
            if ( by_index )
            {
                sorted_orders.insert( pos->getLowestMarketIndex(), pos );
            }
            else
            {
                sorted_orders.insert( pos->price, pos );
            }
        }
    }

    Coin sell_total;
    Coin buy_total;
    Coin hi_buy;
    Coin lo_sell = CoinAmount::A_LOT;

    // print the map
    for ( QMultiMap<Coin/*price/idx*/, Position*>::const_iterator i = sorted_orders.begin(); i != sorted_orders.end(); i++ )
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

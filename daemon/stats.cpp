#include "stats.h"
#include "trexrest.h"
#include "bncrest.h"
#include "polorest.h"
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
    // stringify date + market
    Coin volume_amt = pos->btc_amount.toAmountString(); // cache double
    QString date_str = Global::getDateStringMDY(); // cache mdy
    QString date_market_str = QString( "%1 %2" )
                                .arg( date_str )
                                .arg( pos->market, 8 );

    // stringify and truncate profit margin
    QString profit_margin_trunc = pos->profit_margin.toAmountString();
    profit_margin_trunc.truncate( 6 );

    QString btc_amount_trunc = pos->btc_amount;
    btc_amount_trunc.truncate( 6 );

    // stringify market + profit margin
    QString market_profit_str = QString( "%1 o:%2 pm:%3 %4" )
                                .arg( pos->market, 8 )
                                .arg( btc_amount_trunc )
                                .arg( profit_margin_trunc )
                                .arg( pos->is_landmark ? "L" : " " );

    // stringify date + market + profit margin
    QString date_market_profit_str = QString( "%1 %2 o:%3 pm:%4" )
                                .arg( date_str )
                                .arg( pos->market, 8 )
                                .arg( btc_amount_trunc )
                                .arg( profit_margin_trunc );

    MarketInfo &info = engine->getMarketInfo( pos->market );
    bool market_sentiment = info.market_sentiment;
    qreal market_offset = info.market_offset;

    // update some stats
    //MarketStats &_stats = market_stats[ pos->market ];
    market_volumes[ pos->market ] += volume_amt;
    market_profit[ pos->market ] += pos->per_trade_profit.toAmountString().toDouble();
    market_profit_volume[ market_profit_str ] += volume_amt;
    market_profit_fills[ market_profit_str ]++;
    daily_market_profit_volume[ date_market_profit_str ] += volume_amt;
    daily_market_volume[ date_market_str ] += volume_amt;
    daily_market_profit[ date_market_str ] += pos->per_trade_profit.toAmountString().toDouble();
    daily_volumes[ date_str ] += volume_amt;
    daily_profit[ date_str ] += pos->per_trade_profit.toAmountString().toDouble();
    daily_fills[ date_str ]++;
    last_price[ pos->market ] = pos->price;
    market_fills[ pos->market ]++;
    market_shortlong[ pos->market ] += market_sentiment ?
                                       (  market_offset / 2 ) * pos->btc_amount.toAmountString().toDouble()
                                     : ( -market_offset / 2 ) * pos->btc_amount.toAmountString().toDouble();

    // avoid div0 just incase
    Coin risk_reward_val;
    if ( pos->btc_amount.isGreaterThanZero() )
        risk_reward_val = pos->per_trade_profit / pos->btc_amount;

    // update dailymarketprofitRW
    daily_market_profit_risk_reward[ date_market_profit_str ].first += risk_reward_val;
    daily_market_profit_risk_reward[ date_market_profit_str ].second++;

    // update dailyprofitRW
    market_profit_risk_reward[ market_profit_str ].first += risk_reward_val;
    market_profit_risk_reward[ market_profit_str ].second++;
}

void Stats::clearSome( const QString &market )
{
    if ( market.size() > 0 )
    {
        kDebug() << "clearing strat stats for" << market;

        QStringList market_profit_risk_reward_deleted;
        QStringList daily_market_profit_risk_reward_deleted;
        QStringList daily_market_profit_volume_deleted;
        QStringList market_profit_volume_deleted;

        // remove single market stats from market_profit_risk_reward
        for ( QMap<QString /*day-market-profit*/, QPair<Coin,quint64> /*total, count*/>::const_iterator
              i = market_profit_risk_reward.begin(); i != market_profit_risk_reward.end(); i++ )
        {
            if( i.key().contains( market ) )
                market_profit_risk_reward_deleted.append( i.key() );
        }

        while ( market_profit_risk_reward_deleted.size() > 0 )
            market_profit_risk_reward.remove( market_profit_risk_reward_deleted.takeLast() );
        //

        // remove single market stats from daily_market_profit_risk_reward
        for ( QMap<QString /*day-market-profit*/, QPair<Coin,quint64> /*total, count*/>::const_iterator
              i = daily_market_profit_risk_reward.begin(); i != daily_market_profit_risk_reward.end(); i++ )
        {
            if( i.key().contains( market ) )
                daily_market_profit_risk_reward_deleted.append( i.key() );
        }

        while ( daily_market_profit_risk_reward_deleted.size() > 0 )
            daily_market_profit_risk_reward.remove( daily_market_profit_risk_reward_deleted.takeLast() );
        //

        // remove single market stats from daily_market_profit_volume
        for ( QMap<QString /*market*/, Coin /*volume*/>::const_iterator
              i = daily_market_profit_volume.begin(); i != daily_market_profit_volume.end(); i++ )
        {
            if( i.key().contains( market ) )
                daily_market_profit_volume_deleted.append( i.key() );
        }

        while ( daily_market_profit_volume_deleted.size() > 0 )
            daily_market_profit_volume.remove( daily_market_profit_volume_deleted.takeLast() );
        //

        // remove single market stats from market_profit_volume
        for ( QMap<QString /*market*/, Coin /*volume*/>::const_iterator
              i = market_profit_volume.begin(); i != market_profit_volume.end(); i++ )
        {
            if( i.key().contains( market ) )
                market_profit_volume_deleted.append( i.key() );
        }

        while ( market_profit_volume_deleted.size() > 0 )
            market_profit_volume.remove( market_profit_volume_deleted.takeLast() );
        //
    }
    else
    {
        kDebug() << "clearing strat stats for all";

        market_profit_risk_reward.clear();
        daily_market_profit_risk_reward.clear();
        daily_market_profit_volume.clear();
        market_profit_volume.clear();
    }
}

void Stats::clearAll()
{
    market_volumes.clear();
    market_profit.clear();
    market_profit_fills.clear();
    daily_market_volume.clear();
    daily_market_profit.clear();
    daily_volumes.clear();
    daily_profit.clear();
    market_fills.clear();
    last_price.clear();

    // clear strat stats
    market_profit_risk_reward.clear();
    daily_market_profit_risk_reward.clear();
    daily_market_profit_volume.clear();
    market_profit_volume.clear();
}

void Stats::printOrders( QString market )
{
    QMultiMap<QString/*price*/, Position*> sorted_orders;

    // sort positions_all into map
    for ( QSet<Position*>::const_iterator i = engine->positionsAll().begin(); i != engine->positionsAll().end(); i++ )
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
    for ( QSet<Position*>::const_iterator i = engine->positionsAll().begin(); i != engine->positionsAll().end(); i++ )
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
                        .arg( data.price_lo )
                        .arg( data.price_hi )
                        .arg( data.order_size )
                        .arg( data.fill_count, -3 )
                        .arg( data.alternate_size );
    }
}

void Stats::printVolumes()
{ // print local volumes for all markets
    Coin total_volume = 0.;

    QMap<QString /*market*/, Coin /*volume*/>::const_iterator i;
    for ( i = market_volumes.begin(); i != market_volumes.end(); i++ )
    {
        const QString &market = i.key();
        const Coin &volume = market_volumes.value( market );

        kDebug() << QString( "%1: %2" )
                .arg( market, 8 )
                .arg( volume );

        total_volume += volume;
    }

    kDebug() << "volume total:" << total_volume;
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

void Stats::printFills()
{
    qint64 total = 0;

    QMap<QString /*market*/, qint64 /*total*/>::const_iterator i;
    for ( i = market_fills.begin(); i != market_fills.end(); i++ )
    {
        const QString &market = i.key();
        const qint64 &count = i.value();

        kDebug() << QString( "%1: %2" )
                .arg( market, 8 )
                .arg( count );

        total += count;
    }

    kDebug() << "total fills:" << total;
}

//void Stats::printOrdersTotal()
//{
//    qint64 total = 0;

//    QMap<QString /*market*/, qint32 /*total*/>::const_iterator i;
//    for ( i = daily_orders.begin(); i != daily_orders.end(); i++ )
//    {
//        const QString &market = i.key();
//        const qint64 &count = i.value();

//        kDebug() << QString( "%1: %2" )
//                .arg( market, 8 )
//                .arg( count );

//        total += count;
//    }

//    kDebug() << "total orders:" << total;
//}

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
    for ( QSet<Position*>::const_iterator i = engine->positionsAll().begin(); i != engine->positionsAll().end(); i++ )
    {
        Position *const &pos = *i;

        if ( pos->side == SIDE_BUY )
            buys[ pos->market ]++;
        else if ( pos->side == SIDE_SELL )
            sells[ pos->market ]++;

        // save the total count
        total[ pos->market ]++;
    }

    kDebug() << "buys: " << buys;
    kDebug() << "sells:" << sells;
    kDebug() << "total:" << total;
}

void Stats::printShortLong()
{
    Coin total = 0.;

    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = market_shortlong.begin(); i != market_shortlong.end(); i++ )
    {
        const Coin &val = i.value();

        total += val;
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( val );
    }

    kDebug() << "short long total:" << total;
}

void Stats::printProfit()
{
    Coin total = 0.;

    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = market_profit.begin(); i != market_profit.end(); i++ )
    {
        const Coin &val = i.value();

        total += val;
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( val );
    }

    kDebug() << "profit total:" << total;
}

void Stats::printDailyProfit()
{
    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = daily_profit.begin(); i != daily_profit.end(); i++ )
    {
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( i.value() );
    }
}

void Stats::printMarketProfit()
{
    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = market_profit.begin(); i != market_profit.end(); i++ )
    {
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( i.value() );
    }
}

void Stats::printDailyMarketProfit()
{
    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = daily_market_profit.begin(); i != daily_market_profit.end(); i++ )
    {
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( i.value() );
    }
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

void Stats::printDailyMarketProfitVolume()
{
    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = daily_market_profit_volume.begin(); i != daily_market_profit_volume.end(); i++ )
    {
        kDebug() << QString( "%1: %2" )
                .arg( i.key(), 8 )
                .arg( i.value() );
    }
}

void Stats::printMarketProfitVolume()
{
    QMap<QString /*market*/, Coin /*amount*/>::const_iterator i;
    for ( i = market_profit_volume.begin(); i != market_profit_volume.end(); i++ )
    {
        kDebug() << QString( "%1: %2" )
                .arg( i.key() )
                .arg( i.value() );
    }
}

void Stats::printMarketProfitFills()
{
    QMap<QString /*market*/, qint64 /*total*/>::const_iterator i;
    for ( i = market_profit_fills.begin(); i != market_profit_fills.end(); i++ )
    {
        const QString &market = i.key();
        const qint64 &count = i.value();

        kDebug() << QString( "%1: %2" )
                .arg( market, 8 )
                .arg( count );
    }
}

void Stats::printDailyMarketProfitRW()
{
    QMap<QString /*day-market-profit*/, QPair<Coin,quint64> /*total, count*/>::const_iterator i;
    for ( i = daily_market_profit_risk_reward.begin(); i != daily_market_profit_risk_reward.end(); i++ )
    {
        const QString &market = i.key();
        const Coin &total = i.value().first;
        const quint64 &count = i.value().second;

        kDebug() << QString( "%1 %2" )
                .arg( market, 8 )
                .arg( total / count );
    }
}

void Stats::printMarketProfitRW()
{
    QMap<QString /*market-profit*/, QPair<Coin,quint64> /*r-w ratios*/>::const_iterator i;
    for ( i = market_profit_risk_reward.begin(); i != market_profit_risk_reward.end(); i++ )
    {
        const QString &market_profit_str = i.key();
        const Coin &total = i.value().first;
        const quint64 &count = i.value().second;

        const Coin &volume = market_profit_volume.value( market_profit_str );

        kDebug() << QString( "%1 rw:%2 vol:%3" )
                .arg( market_profit_str, 8 )
                .arg( total / count )
                .arg( volume );
    }
}

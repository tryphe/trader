#include "global.h"
#include "positionman.h"
#include "polorest.h"
#include "trexrest.h"
#include "bncrest.h"
#include "engine.h"
#include "enginesettings.h"

PositionMan::PositionMan( Engine *_engine, QObject *parent )
    : QObject( parent ),
      engine( _engine )
{
    kDebug() << "[PositionMan]";
}

PositionMan::~PositionMan()
{
    // delete local positions
    while( positions_all.size() > 0 )
        remove( *positions_all.begin() );
}

bool PositionMan::hasActivePositions() const
{
    return positions_active.size();
}

bool PositionMan::hasQueuedPositions() const
{
    return positions_queued.size();
}

bool PositionMan::isActive( Position * const &pos ) const
{
    return positions_active.contains( pos );
}

bool PositionMan::isQueued( Position * const &pos ) const
{
    return positions_queued.contains( pos );
}

bool PositionMan::isValid( Position * const &pos ) const
{
    return positions_all.contains( pos );
}

bool PositionMan::isValidOrderID( const QString &order_id ) const
{
    return positions_by_number.contains( order_id );
}

Position *PositionMan::getByOrderID( const QString &order_id ) const
{
    return positions_by_number.value( order_id, nullptr );
}

Coin PositionMan::getHiBuyFlipPrice( const QString &market ) const
{
    Position *pos = getHighestBuyByPrice( market );

    // check pos
    if ( !pos || !isActive( pos ) )
        return 0.;

    kDebug() << "hi_buy_flip" << pos->stringifyOrder();

    return pos->sell_price;
}

Coin PositionMan::getLoSellFlipPrice( const QString &market ) const
{
    Position *pos = getLowestSellByPrice( market );

    // check pos
    if ( !pos || !isActive( pos ) )
        return 0.;

    kDebug() << "lo_sell_flip" << pos->stringifyOrder();

    return pos->buy_price;
}

Position *PositionMan::getByIndex( const QString &market, const qint32 idx ) const
{
    Position *ret = nullptr;

    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // check for idx
        if ( pos->market == market && pos->market_indices.contains( idx ) )
            return pos;
    }

    return ret;
}

Position *PositionMan::getHighestBuyAll( const QString &market ) const
{
    Position *ret = nullptr;
    Coin hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->is_invalidated ||
              pos->side != SIDE_BUY ||          // sells only
              pos->market != market             // check market filter
              )
            continue;

        if ( pos->buy_price > hi_buy ) // position index is greater than our incrementor
        {
            hi_buy = pos->buy_price;
            ret = pos;
        }
    }

    return ret;
}

Position *PositionMan::getLowestSellAll( const QString &market ) const
{
    Position *ret = nullptr;
    Coin lo_sell = CoinAmount::A_LOT;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->is_invalidated ||
              pos->side != SIDE_SELL ||         // sells only
              pos->market != market             // check market filter
              )
            continue;

        if ( pos->sell_price < lo_sell ) // position index is less than our incrementor
        {
            lo_sell = pos->sell_price;
            ret = pos;
        }
    }

    return ret;
}

bool PositionMan::isDivergingConverging( const QString &market, const qint32 index ) const
{
    return diverging_converging[ market ].contains( index );
}

Position *PositionMan::getHighestBuyByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_BUY ||         // sells only
              pos->is_invalidated ||
              pos->is_cancelling ||             // must not be cancelling
              pos->order_number.size() == 0 ||  // must be set
              pos->market != market             // check market filter
              )
            continue;

        const qint32 pos_idx = pos->getHighestMarketIndex();
        if ( pos_idx > idx_hi_buy ) // position index is greater than our incrementor
        {
            idx_hi_buy = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *PositionMan::getHighestSellByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_SELL ||         // sells only
              pos->is_invalidated ||
              pos->is_cancelling ||             // must not be cancelling
              pos->order_number.size() == 0 ||  // must be set
              pos->market != market             // check market filter
              )
            continue;

        const qint32 pos_idx = pos->getHighestMarketIndex();
        if ( pos_idx > idx_hi_buy ) // position index is greater than our incrementor
        {
            idx_hi_buy = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *PositionMan::getLowestSellByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_lo_sell = std::numeric_limits<qint32>::max();

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_SELL ||         // sells only
              pos->is_invalidated ||
              pos->is_cancelling ||             // must not be cancelling
              pos->order_number.size() == 0 ||  // must be set
              pos->market != market             // check market filter
              )
            continue;
        const qint32 pos_idx = pos->getLowestMarketIndex();
        if ( pos_idx < idx_lo_sell ) // position index is greater than our incrementor
        {
            idx_lo_sell = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *PositionMan::getLowestBuyByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_lo_sell = std::numeric_limits<qint32>::max();

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_BUY ||          // sells only
              pos->is_invalidated ||
              pos->is_cancelling ||             // must not be cancelling
              pos->order_number.size() == 0 ||  // must be set
              pos->market != market             // check market filter
              )
            continue;

        const qint32 pos_idx = pos->getLowestMarketIndex();
        if ( pos_idx < idx_lo_sell ) // position index is greater than our incrementor
        {
            idx_lo_sell = pos_idx;
            ret = pos;
        }
    }

    return ret;
}

Position *PositionMan::getHighestBuyByPrice( const QString &market ) const
{
    Position *ret = nullptr;
    Coin hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_BUY ||          // sells only
              pos->is_invalidated ||
              pos->is_cancelling ||             // must not be cancelling
              pos->order_number.size() == 0 ||  // must be set
              pos->market != market             // check market filter
              )
            continue;

        if ( pos->buy_price > hi_buy ) // position index is greater than our incrementor
        {
            hi_buy = pos->buy_price;
            ret = pos;
        }
    }

    return ret;
}

Position *PositionMan::getLowestSellByPrice( const QString &market ) const
{
    Position *ret = nullptr;
    Coin lo_sell = CoinAmount::A_LOT;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_SELL ||         // sells only
              pos->is_invalidated ||
              pos->is_cancelling ||             // must not be cancelling
              pos->order_number.size() == 0 ||  // must be set
              pos->market != market             // check market filter
              )
            continue;

        if ( pos->sell_price < lo_sell ) // position index is less than our incrementor
        {
            lo_sell = pos->sell_price;
            ret = pos;
        }
    }

    return ret;
}

Position *PositionMan::getLowestPingPong( const QString &market ) const
{
    // store lo and lo pointer
    Position *lo_pos = nullptr;
    qint32 lo_idx = std::numeric_limits<qint32>::max();

    // look for highest position for a market
    qint32 pos_lo_idx;
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // note: cancelLowest() uses this. we must exlude one-time orders otherwise automatic ping-pong
        //       maintenance interferes with one-time orders.
        if ( pos->is_onetime ||
             pos->is_invalidated )
            continue;

        pos_lo_idx = pos->getLowestMarketIndex();

        if ( pos_lo_idx < lo_idx &&
            !pos->is_cancelling &&
             pos->market == market )
        {
            lo_idx = pos_lo_idx;
            lo_pos = pos;
        }
    }

    return lo_pos;
}

Position *PositionMan::getHighestPingPong( const QString &market ) const
{
    // store hi and high pointer
    Position *hi_pos = nullptr;
    qint32 hi_idx = -1;

    // look for highest sell index for a market
    qint32 pos_hi_idx;
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // note: cancelHighest() uses this. we must exlude one-time orders otherwise automatic ping-pong
        //       maintenance interferes with one-time orders.
        if ( pos->is_onetime ||
             pos->is_invalidated )
            continue;

        pos_hi_idx = pos->getHighestMarketIndex();

        if ( pos_hi_idx > hi_idx &&
            !pos->is_cancelling &&
             pos->market == market )
        {
            hi_idx = pos_hi_idx;
            hi_pos = pos;
        }
    }

    return hi_pos;
}

qint32 PositionMan::getLowestPingPongIndex( const QString &market ) const
{
    qint32 new_index = std::numeric_limits<qint32>::max();

    // get lowest sell from all positions
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // skip if one-time order
        if ( pos->is_onetime ||
             pos->is_invalidated )
            continue;

        const qint32 pos_lowest_idx = pos->getLowestMarketIndex();

        if ( pos_lowest_idx < new_index &&
            !pos->is_cancelling &&
             pos->market == market )
        {
            new_index = pos_lowest_idx;
        }
    }

    return new_index;
}

qint32 PositionMan::getHighestPingPongIndex( const QString &market ) const
{
    qint32 new_index = -1;

    // look for the highest buy index
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        // skip if one-time order
        if ( pos->is_onetime ||
             pos->is_invalidated )
            continue;

        const qint32 pos_highest_idx = pos->getHighestMarketIndex();

        if ( pos_highest_idx > new_index &&
            !pos->is_cancelling &&
             pos->market == market )
            new_index = pos_highest_idx;
    }

    return new_index;
}

qint32 PositionMan::getMarketOrderTotal( const QString &market, bool onetime_only ) const
{
    if ( market.isEmpty() )
        return 0;

    qint32 total = 0;

    // get total order count for a market
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        const Position *const &pos = *i;

        // onetime_only, only include onetime orders
        if ( onetime_only && !pos->is_onetime )
            continue;

        if ( !pos->is_invalidated &&
              pos->market == market )
            total++;
    }

    return total;
}

qint32 PositionMan::getBuyTotal( const QString &market ) const
{
    if ( market.isEmpty() )
        return 0;

    qint32 total = 0;

    // get total order count for a market
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        if ( pos->side == SIDE_BUY &&
            !pos->is_invalidated &&
             pos->market == market )
            total++;
    }

    return total;
}

void PositionMan::flipLoSellIndex( const QString &market, QString tag )
{
    Position *pos = getLowestSellByIndex( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued long     %1" )
                  .arg( pos->stringifyPositionChange() );

    cancel( pos, false, CANCELLING_FOR_SHORTLONG );
}

Coin PositionMan::getLoSell( const QString &market ) const
{
    return engine->market_info[ market ].lowest_sell;
}

Coin PositionMan::getHiBuy( const QString &market ) const
{
    return engine->market_info[ market ].highest_buy;
}

qint32 PositionMan::getSellTotal( const QString &market ) const
{
    if ( market.isEmpty() )
        return 0;

    qint32 total = 0;

    // get total order count for a market
    for ( QSet<Position*>::const_iterator i = positions_all.begin(); i != positions_all.end(); i++ )
    {
        Position *const &pos = *i;

        if ( pos->side == SIDE_SELL &&
             pos->market == market )
            total++;
    }

    return total;
}

void PositionMan::flipHiBuyPrice( const QString &market, QString tag )
{
    Position *pos = getHighestBuyByPrice( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued short    %1" )
                  .arg( pos->stringifyPositionChange() );

    cancel( pos, false, CANCELLING_FOR_SHORTLONG );
}

void PositionMan::flipHiBuyIndex( const QString &market, QString tag )
{
    Position *pos = getHighestBuyByIndex( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued short    %1" )
                  .arg( pos->stringifyPositionChange() );

    cancel( pos, false, CANCELLING_FOR_SHORTLONG );
}

void PositionMan::flipLoSellPrice( const QString &market, QString tag )
{
    Position *pos = getLowestSellByPrice( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued long     %1" )
                  .arg( pos->stringifyPositionChange() );

    cancel( pos, false, CANCELLING_FOR_SHORTLONG );
}



void PositionMan::add( Position * const &pos )
{
    positions_queued.insert( pos );
    positions_all.insert( pos );
}

void PositionMan::activate( Position * const &pos, const QString &order_number )
{
    if ( !isValid( pos ) || order_number.isEmpty() )
    {
        kDebug() << "local error: tried to set order with blank orderid" << pos->stringifyOrder();
        return;
    }

    // set the order_set_time so we can keep track of a missing order
    pos->order_set_time = QDateTime::currentMSecsSinceEpoch();

    // the order is set, unflag as new order if set
    pos->is_new_hilo_order = false;

    // insert our order number into positions
    positions_queued.remove( pos );
    positions_active.insert( pos );
    positions_by_number.insert( order_number, pos );

    if ( engine->is_testing )
    {
        pos->order_number = order_number;
    }
    else
    {
        // on binance, prepend market to orderid for uniqueness (don't remove this or you'll get collisions)
        #if defined(EXCHANGE_BINANCE)
        pos->order_number = pos->market + order_number;
        #else
        pos->order_number = order_number;
        #endif
    }

    // print set order
    if ( engine->verbosity > 0 )
        kDebug() << QString( "%1 %2" )
                    .arg( "set", -15 )
                    .arg( pos->stringifyOrder() );

    // check if the order was queued for a cancel (manual or automatic) while it was queued
    if ( pos->is_cancelling &&
         pos->order_cancel_time < QDateTime::currentMSecsSinceEpoch() - engine->settings->cancel_timeout )
    {
        cancel( pos, true, pos->cancel_reason );
    }
}

void PositionMan::remove( Position * const &pos )
{
    // prevent invalid access if pos is bad
    if ( !positions_active.contains( pos ) && !positions_queued.contains( pos ) )
    {
        kDebug() << "local error: called deletePosition with invalid position at" << &*pos;
        return;
    }

    /// step 1: clear position from diverge/converge, if we were diverging/converging
    removeFromDC( pos );

    /// step 2: make sure there's no active requests for this position
    QQueue<QPair<QNetworkReply*,Request*>> deleted_queue;

    // check nam_sent_queue for requests over timeout
    for ( QHash<QNetworkReply*,Request*>::const_iterator i = engine->rest->nam_queue_sent.begin(); i != engine->rest->nam_queue_sent.end(); i++ )
    {
        const Request *const &req = i.value();

        if ( !req->pos )
            continue;

        // if we found -this- position, add it to deleted queue
        if ( req->pos == pos )
            deleted_queue.append( qMakePair( i.key(), i.value() ) );
    }

    // delete replies that we handled
    while ( deleted_queue.size() > 0 )
    {
        QPair<QNetworkReply*,Request*> pair = deleted_queue.takeFirst();
        engine->deleteReply( pair.first, pair.second );
    }

    /// step 3: remove from maps/containers
    positions_active.remove( pos ); // remove from active ptr list
    positions_queued.remove( pos ); // remove from tracking queue
    positions_all.remove( pos ); // remove from all
    positions_by_number.remove( pos->order_number ); // remove order from positions
    engine->market_info[ pos->market ].order_prices.removeOne( pos->price ); // remove from prices

    delete pos; // we're done with this on the heap
}

void PositionMan::removeFromDC( Position * const &pos )
{
    // pos must be valid!

    // remove position from reverse lookup dc list
    for ( QMap<QVector<Position*>, QPair<bool, QVector<qint32>>>::const_iterator i = diverge_converge.begin(); i != diverge_converge.end(); i++ )
    {
        const QVector<Position*> &positions = i.key();

        // remove the key,val from the map so it doesn't hang around
        if ( positions.contains( pos ) )
        {
            diverge_converge.remove( positions );
            break;
        }
    }

    // remove order indices from dc list that contains indices that are converging/diverging
    for ( int i = 0; i < pos->market_indices.size(); i++ )
        diverging_converging[ pos->market ].removeOne( pos->market_indices.value( i ) );
}

void PositionMan::cancelAll( QString market )
{
    // the arg will always be supplied; set the default arg here instead of the function def
    if ( market.isEmpty() )
        market = ALL;

    // safety check to avoid s-filling local positions
    if ( ( hasActivePositions() || hasQueuedPositions() )
         && market == ALL )
    {
        kDebug() << "local error: you have open positions, did you mean cancellocal?";
        return;
    }

    // clear market index
    if ( market == ALL )
    {
        for ( QHash<QString, MarketInfo>::iterator i = engine->market_info.begin(); i != engine->market_info.end(); i++ )
        {
            (*i).order_prices.clear();
            (*i).position_index.clear();
        }
        kDebug() << "cleared all market indexes";
    }
    else
    {
        engine->market_info[ market ].order_prices.clear();
        engine->market_info[ market ].position_index.clear();
        kDebug() << "cleared" << market << "market index";
    }

    engine->is_running_cancelall = true;
    engine->cancel_market_filter = market;

#if defined(EXCHANGE_BITTREX)
    engine->rest->sendRequest( TREX_COMMAND_GET_ORDERS );
#elif defined(EXCHANGE_BINANCE)
    engine->rest->sendRequest( BNC_COMMAND_GETORDERS, "", nullptr, 40 );
#elif defined(EXCHANGE_POLONIEX)
    engine->rest->sendRequest( POLO_COMMAND_GETORDERS, POLO_COMMAND_GETORDERS_ARGS );
#endif
}

void PositionMan::cancelLocal( QString market )
{
    // the arg will always be supplied; set the default arg here instead of the function def
    if ( market.isEmpty() )
        market = ALL;

    // copy the list so we can iterate and delete freely
    QQueue<Position*> normal_positions;
    QQueue<Position*> landmark_positions;
    QQueue<Position*> deleted_positions;

    // cancel orders if we matched the market
    for ( QSet<Position*>::const_iterator i = all().begin(); i != all().end(); i++ )
    {
        Position *const &pos = *i;

        // we must match or filter, or it must be null/empty
        if ( market == ALL || pos->market == market )
        {
            // delete queued positions
            if ( isQueued( pos ) )
                deleted_positions.append( pos );
            else if ( pos->is_landmark )
                landmark_positions.append( pos );
            else
                normal_positions.append( pos );
        }
    }

    // delete queued positions
    while ( deleted_positions.size() > 0 )
        remove( deleted_positions.takeFirst() );

    // delete and cancel all normal positions
    while ( normal_positions.size() > 0 )
        cancel( normal_positions.takeFirst() );

    // delete and cancel all landmark positions
    while ( landmark_positions.size() > 0 )
        cancel( landmark_positions.takeFirst() );

    // clear market index
    if ( market == ALL )
    {
        for ( QHash<QString, MarketInfo>::iterator i = engine->market_info.begin(); i != engine->market_info.end(); i++ )
        {
            (*i).order_prices.clear();
            (*i).position_index.clear();
        }
    }
    else
    {
        engine->market_info[ market ].order_prices.clear();
        engine->market_info[ market ].position_index.clear();
    }

    if ( !engine->is_testing )
        kDebug() << "cleared" << market << "market indices";
}

void PositionMan::cancel( Position *const &pos, bool quiet, quint8 cancel_reason )
{
    // check for position in ptr list
    if ( !pos || !isValid( pos ) )
    {
        kDebug() << "local error: aborting dangerous cancel not found in positions_all";
        return;
    }

    // if testing, skip ahead to processCancelledOrder logic which just calls remove();
    if ( engine->is_testing )
    {
        remove( pos );
        return;
    }

    // flag if the order was cancelling already
    const bool recancelling = pos->order_cancel_time > 0 || pos->is_cancelling;

    // set cancel reason (override if neccesary to change reason)
    pos->cancel_reason = cancel_reason;

    // check if this is a queued position so we can properly cancel the order when it gets set
    if ( isQueued( pos ) )
    {
        // let the order timeout if it doesn't have an orderid, but only after it gets set
        pos->is_cancelling = true;
        pos->order_cancel_time = 1; // set cancel time >0 to trip the next timeout check
        //kDebug() << "local warning: trying to cancel order, but order is in flight";
        return;
    }

    if ( !quiet )
    {
        const QString prefix_str = QString( "%1%2" )
                    .arg( pos->is_onetime ? "cancelling" :
                          pos->is_slippage ? "resetting " :
                          recancelling ?     "recancelling   " : "cancelling" )
                    .arg( cancel_reason == CANCELLING_LOWEST             ? " lo  " :
                          cancel_reason == CANCELLING_HIGHEST            ? " hi  " :
                          cancel_reason == CANCELLING_FOR_MAX_AGE        ? " age " :
                          cancel_reason == CANCELLING_FOR_SHORTLONG      ? " s/l " :
                                                                           "" ); // CANCELLING_FOR_SLIPPAGE_RESET

        kDebug() << QString( "%1 %2" )
                    .arg( prefix_str, -15 )
                    .arg( pos->stringifyOrder() );
    }

    // send request
    engine->rest->sendCancel( pos->order_number, pos );
}

void PositionMan::cancelHighest( const QString &market )
{
    // store hi and high pointer
    Position *const &hi_pos = getHighestPingPong( market );

    // cancel highest order
    if ( hi_pos )
        cancel( hi_pos, false, CANCELLING_HIGHEST );
}

void PositionMan::cancelLowest( const QString &market )
{
    // store lo and lo pointer
    Position *const &lo_pos = getLowestPingPong( market );

    // cancel lowest order
    if ( lo_pos )
        cancel( lo_pos, false, CANCELLING_LOWEST );
}

void PositionMan::checkBuySellCount()
{
    static QMap<QString /*market*/, qint32> buys, sells;
    buys.clear();
    sells.clear();

    // look for highest index in active and queued positions
    for ( QSet<Position*>::const_iterator i = all().begin(); i != all().end(); i++ )
    {
        const Position *const &pos = *i;
        const QString &market = pos->market;

        if ( market.isEmpty() )
            continue;
        // tally buys
        else if ( pos->side == SIDE_BUY && !pos->is_cancelling )
            buys[ market ]++;
        // tally sells
        else if ( pos->side == SIDE_SELL && !pos->is_cancelling )
            sells[ market ]++;
    }

    // run until we stop setting new orders or flow control returns
    const QList<QString> &markets = engine->market_info.keys();
    quint16 new_orders_ct;
    do
    {
        new_orders_ct = 0;

        // check buy counts
        for ( QList<QString>::const_iterator i = markets.begin(); i != markets.end(); i++ )
        {
            const QString &market = *i;
            const MarketInfo &info = engine->market_info[ market ];
            const qint32 &order_min = info.order_min;
            const qint32 &order_max = info.order_max;
            qint32 buy_count = buys[ market ];
            qint32 sell_count = sells[ market ];

            // if we are cancelling, don't set more orders
            if ( info.position_index.isEmpty() )
                continue;

            // allow skipping of automation by setting a market min/max to 0
            if ( order_min <= 0 || order_max <= 0 )
                continue;

            /// check buy counts
            // did we reach the max orders for this market?
            while ( buy_count > order_max )
            {
                cancelLowest( market );
                buys[ market ]--;
                buy_count--;

                // flow control
                if ( engine->rest->yieldToFlowControl() )
                    return;
            }

            // count < min
            if ( buy_count < order_min )
            {
                setNextLowest( market );
                buys[ market ]++;
                new_orders_ct++;
            }
            // count >= min and count < max
            else if ( info.order_dc > 1 &&
                 buy_count >= order_min &&
                 buy_count < order_max - info.order_landmark_thresh )
            {
                setNextLowest( market, SIDE_BUY, true );
                buys[ market ]++;
                new_orders_ct++;
            }

            // flow control
            if ( engine->rest->yieldToFlowControl() )
                return;
            ///

            /// check sell counts
            // did we reach the max orders for this market?
            while ( sell_count > order_max )
            {
                cancelHighest( market );
                sells[ market ]--;
                sell_count--;

                // flow control
                if ( engine->rest->yieldToFlowControl() )
                    return;
            }

            // count < min
            if ( sell_count < order_min )
            {
                setNextHighest( market );
                sells[ market ]++;
                new_orders_ct++;
            }
            // count >= min and count < max
            else if ( info.order_dc > 1 &&
                 sell_count >= order_min &&
                 sell_count < order_max - info.order_landmark_thresh )
            {
                setNextHighest( market, SIDE_SELL, true );
                sells[ market ]++;
                new_orders_ct++;
            }

            // flow control
            if ( engine->rest->yieldToFlowControl() )
                return;
            ///
        }
    }
    while( new_orders_ct > 0 );

    buys.clear();
    sells.clear();
}

void PositionMan::setNextLowest( const QString &market, quint8 side, bool landmark )
{
    // somebody fucked up
    if ( side != SIDE_SELL && side != SIDE_BUY )
    {
        kDebug() << "local error: invalid order 'side'";
        return;
    }

    qint32 new_index = getLowestPingPongIndex( market );

    // subtract 1
    new_index--;

    // check for index ok
    if ( new_index < 0 || new_index > std::numeric_limits<qint32>::max() -2 )
        return;

    const MarketInfo &info = engine->market_info[ market ];
    const qint32 dc_val = info.order_dc;

    // count down until we find an index without a position
    while ( getByIndex( market, new_index ) ||
            isDivergingConverging( market, new_index ) )
        new_index--;

    QVector<qint32> indices = QVector<qint32>() << new_index;

    // check if we ran out of indexed positions so we don't make a bogus order
    if ( indices.value( 0 ) < 0 )
        return; // we need at least 1 valid index

    // add an index until we run out of bounds or our landmark size is matched
    while ( landmark && indices.size() < dc_val )
    {
        new_index = indices.value( indices.size() -1 ) -1;

        // are we about to add an out of bounds index?
        if ( new_index < 0 )
        {
            // preserve the indices and break here
            break;
        }

        // if we can't use the new index, go to the -next lowest- index and restart the loop
        if ( getByIndex( market, new_index ) ||
             isDivergingConverging( market, new_index ) )
        {
            while ( indices.size() > 1 )
                indices.removeLast();

            break;
        }

        indices.append( new_index );
    }

    // enforce full landmark size or return, except on the boundary of our positions
    if ( ( landmark && indices.size() != dc_val ) &&  // tried to set a landmark order with the wrong size
         !indices.contains( 0 ) ) // contains lowest position index
        return;

    // enforce return on normal order with >1 size
    if ( !landmark && indices.size() > 1 )
        return;

    // check for out of bounds indices[0] and [n]
    if ( indices.isEmpty() ||
         indices.value( 0 ) >= info.position_index.size() )
        return;

    // get the index data
    const PositionData &data = info.position_index.value( indices.value( 0 ) );

//    kDebug() << "adding idx" << indices.value( 0 ) << "from indices" << indices;
//    kDebug() << "adding next lo pos" << market << side << data.buy_price << data.sell_price << data.order_size;

    Position *pos = engine->addPosition( market, side, data.buy_price, data.sell_price, data.order_size, "active", "",
                                 indices, landmark, true );

    // check for valid ptr
    if ( !pos )
        return;

    // flag as non-profitable api call (it's far from the spread)
    pos->is_new_hilo_order = true;

    kDebug() << QString( "setting next lo %1" )
                 .arg( pos->stringifyNewPosition() );
}

void PositionMan::setNextHighest( const QString &market, quint8 side, bool landmark )
{
    // somebody fucked up
    if ( side != SIDE_SELL && side != SIDE_BUY )
    {
        kDebug() << "local error: invalid order 'side'";
        return;
    }

    qint32 new_index = getHighestPingPongIndex( market );

    // add 1
    new_index++;

    // check for index ok
    if ( new_index < 1 || new_index > std::numeric_limits<qint32>::max() -1 )
        return;

    const MarketInfo &info = engine->market_info[ market ];
    const qint32 dc_val = info.order_dc;

    // count up until we find an index without a position
    while ( getByIndex( market, new_index ) ||
            isDivergingConverging( market, new_index ) )
        new_index++;

    QVector<qint32> indices = QVector<qint32>() << new_index;

    // check if we ran out of indexed positions
    if ( indices.value( 0 ) >= info.position_index.size() )
        return;

    // add an index until we run out of bounds or our landmark size is matched
    while ( landmark && indices.size() < dc_val )
    {
        new_index = indices.value( indices.size() -1 ) +1;

        // are we about to add an out of bounds index or one that already exists?
        if ( new_index >= info.position_index.size() )
        {
            // preserve the indices and break here
            break;
        }

        // if we can't use the new index, find the next valid index and restart the loop
        if ( getByIndex( market, new_index ) ||
             isDivergingConverging( market, new_index ) )
        {
            while ( indices.size() > 1 )
                indices.removeLast();

            break;
        }

        indices.append( new_index );
    }

    // enforce full landmark size or return, except on the boundary of our positions
    if ( ( landmark && indices.size() != dc_val ) &&  // tried to set a landmark order with the wrong size
         !indices.contains( info.position_index.size() -1 ) ) // contains highest position index
        return;

    // enforce return on normal order with >1 size
    if ( !landmark && indices.size() > 1 )
        return;

    // check for out of bounds indices[0] and [n]
    if ( indices.isEmpty() ||
         indices.value( 0 ) >= info.position_index.size() )
        return;

    // get the index data
    const PositionData &data = info.position_index.value( indices.value( 0 ) );

//    kDebug() << "adding next hi pos" << market << side << data.buy_price << data.sell_price << data.order_size;

    Position *pos = engine->addPosition( market, side, data.buy_price, data.sell_price, data.order_size, "active", "",
                                     indices, landmark, true );

    // check for valid ptr
    if ( !pos )
        return;

    // flag as non-profitable api call (it's far from the spread)
    pos->is_new_hilo_order = true;

    kDebug() << QString( "setting next hi %1" )
                .arg( pos->stringifyNewPosition() );
}

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

bool PositionMan::isPosition( Position * const &pos ) const
{
    return positions_all.contains( pos );
}

bool PositionMan::isPositionOrderID( const QString &order_id ) const
{
    return positions_by_number.contains( order_id );
}

Position *PositionMan::getPositionForOrderID( const QString &order_id ) const
{
    return positions_by_number.value( order_id, nullptr );
}

Coin PositionMan::getHiBuyFlipPrice( const QString &market ) const
{
    Position *pos = getHighestActiveBuyPosByPrice( market );

    // check pos
    if ( !pos || !isActive( pos ) )
        return 0.;

    kDebug() << "hi_buy_flip" << pos->stringifyOrder();

    return pos->sell_price;
}

Coin PositionMan::getLoSellFlipPrice( const QString &market ) const
{
    Position *pos = getLowestActiveSellPosByPrice( market );

    // check pos
    if ( !pos || !isActive( pos ) )
        return 0.;

    kDebug() << "lo_sell_flip" << pos->stringifyOrder();

    return pos->buy_price;
}

Position *PositionMan::getPositionByIndex( const QString &market, const qint32 idx ) const
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

bool PositionMan::isDivergingConverging( const QString &market, const qint32 index ) const
{
    return diverging_converging[ market ].contains( index );
}

Position *PositionMan::getHighestActiveBuyPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_BUY ||         // sells only
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

Position *PositionMan::getHighestActiveSellPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_SELL ||         // sells only
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

Position *PositionMan::getLowestActiveSellPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_lo_sell = std::numeric_limits<qint32>::max();

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_SELL ||         // sells only
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

Position *PositionMan::getLowestActiveBuyPosByIndex( const QString &market ) const
{
    Position *ret = nullptr;
    qint32 idx_lo_sell = std::numeric_limits<qint32>::max();

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_BUY ||          // sells only
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

Position *PositionMan::getHighestActiveBuyPosByPrice( const QString &market ) const
{
    Position *ret = nullptr;
    Coin hi_buy = -1;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_BUY ||          // sells only
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

Position *PositionMan::getLowestActiveSellPosByPrice( const QString &market ) const
{
    Position *ret = nullptr;
    Coin lo_sell = CoinAmount::A_LOT;

    for ( QSet<Position*>::const_iterator i = positions_active.begin(); i != positions_active.end(); i++ )
    {
        Position *const &pos = *i;
        if (  pos->side != SIDE_SELL ||         // sells only
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

Position *PositionMan::getLowestActivePingPong( const QString &market ) const
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
        if ( pos->is_onetime )
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

Position *PositionMan::getHighestActivePingPong( const QString &market ) const
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
        if ( pos->is_onetime )
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
        if ( pos->is_onetime )
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
        if ( pos->is_onetime )
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

        if ( pos->market == market )
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
             pos->market == market )
            total++;
    }

    return total;
}

void PositionMan::flipLoSellIndex( const QString &market, QString tag )
{
    Position *pos = getLowestActiveSellPosByIndex( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued long     %1" )
                  .arg( pos->stringifyPositionChange() );

    engine->cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
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
    Position *pos = getHighestActiveBuyPosByPrice( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued short    %1" )
                  .arg( pos->stringifyPositionChange() );

    engine->cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
}

void PositionMan::flipHiBuyIndex( const QString &market, QString tag )
{
    Position *pos = getHighestActiveBuyPosByIndex( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued short    %1" )
                  .arg( pos->stringifyPositionChange() );

    engine->cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
}

void PositionMan::flipLoSellPrice( const QString &market, QString tag )
{
    Position *pos = getLowestActiveSellPosByPrice( market );

    // check pos
    if ( !isActive( pos ) )
        return;

    pos->strategy_tag = tag;
    pos->per_trade_profit = Coin(); // clear trade profit from message
    kDebug() << QString( "queued long     %1" )
                  .arg( pos->stringifyPositionChange() );

    engine->cancelOrder( pos, false, CANCELLING_FOR_SHORTLONG );
}



void PositionMan::add( Position * const &pos )
{
    positions_queued.insert( pos );
    positions_all.insert( pos );
}

void PositionMan::activate( Position * const &pos, const QString &order_number )
{
    if ( !isPosition( pos ) || order_number.isEmpty() )
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
        engine->cancelOrder( pos, true, pos->cancel_reason );
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


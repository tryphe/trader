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

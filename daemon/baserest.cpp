#include "baserest.h"
#include "misctypes.h"
#include "global.h"
#include "engine.h"
#include "bncrest.h"
#include "trexrest.h"
#include "polorest.h"

BaseREST::BaseREST(Engine *_engine)
    : QObject( nullptr ),
      request_nonce( 0 ),
      last_request_sent_ms( 0 ),
      orderbook_update_time( QDateTime::currentMSecsSinceEpoch() ), // last orderbook update time
      orderbook_update_request_time( 0 ),
      orderbook_public_update_time( 0 ),
      orderbook_public_update_request_time( 0 ),
      limit_commands_queued( 35 ), // stop checks if we are over this many commands queued
      limit_commands_queued_dc_check( 10 ), // exit dc check if we are over this many commands queued
      limit_commands_sent( 60 ), // stop checks if we are over this many commands sent
      limit_timeout_yield( 12 ),
      market_cancel_thresh( 300 ), // limit for market order total for weighting cancels to be sent first
      slippage_stale_time( 500 ), // (if post-only is enabled, this is 0) quiet time before we allow an order to be included in slippage price calculations
      orderbook_stale_tolerance( 10000 ), // only accept orderbooks sent within this time
      orders_stale_trip_count( 0 ), // state
      books_stale_trip_count( 0 ), // state
      send_timer( nullptr ),
      timeout_timer( nullptr ),
      orderbook_timer( nullptr ),
      diverge_converge_timer( nullptr ),
      ticker_timer( nullptr ),
      nam( nullptr ),
      stats( nullptr ), // do not use until after constructor is finished, initialization happens out of class
      engine( _engine )
{
    kDebug() << "[BaseREST]";
    nam = new QNetworkAccessManager();
}

BaseREST::~BaseREST()
{
    // make sure secret/key is gone
    keystore.clear();
    request_nonce = 0;

    // stop timers
    send_timer->stop();
    timeout_timer->stop();
    orderbook_timer->stop();
    diverge_converge_timer->stop();
    ticker_timer->stop();

    // clear network replies
    for ( QHash<QNetworkReply*,Request*>::const_iterator i = nam_queue_sent.begin(); i != nam_queue_sent.end(); i++ )
    {
        QNetworkReply *reply = i.key();
        reply->setParent( this );
        reply->deleteLater();
    }

    // delete nam queues
    while ( nam_queue.size() > 0 )
        delete nam_queue.takeFirst();

    // force nam to close
    nam->thread()->exit();
    delete nam;
    nam = nullptr;

    delete send_timer;
    delete timeout_timer;
    delete orderbook_timer;
    delete diverge_converge_timer;
    delete ticker_timer;
    send_timer = nullptr;
    timeout_timer = nullptr;
    orderbook_timer = nullptr;
    diverge_converge_timer = nullptr;
    ticker_timer = nullptr;

    stats = nullptr;
    engine = nullptr;

    kDebug() << "[BaseREST] done.";
}

void BaseREST::sendRequest( QString api_command, QString body, Position *pos, quint16 weight )
{
    Request *delayed_request = new Request();
    delayed_request->api_command = api_command;
    delayed_request->body = body;
    delayed_request->pos = pos;
    delayed_request->weight = weight;

    // append to packet queue
    nam_queue.append( delayed_request );

    if ( !send_timer )
        return;

    // if we didn't send a command lately, just trigger the timer and restart the interval
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    if ( last_request_sent_ms <= current_time - send_timer->interval() )
    {
        engine->getRest()->sendNamQueue();
        send_timer->start();
    }
}

bool BaseREST::isKeyOrSecretUnset() const
{
    // return on unset key
    if ( keystore.isKeyOrSecretEmpty() )
    {
        kDebug() << "local warning: key or secret is empty";
        return true;
    }

    return false;
}

bool BaseREST::isCommandQueued( const QString &api_command ) const
{
    // check for getopenorders request in nam_queue
    for ( QQueue<Request*>::const_iterator i = nam_queue.begin(); i != nam_queue.end(); i++ )
        if ( (*i)->api_command == api_command )
            return true;

    return false;
}

bool BaseREST::isCommandSent( const QString &api_command, qint32 min_times ) const
{
    qint32 times = 0;

    // check for getopenorders request in nam_queue
    for ( QHash<QNetworkReply*,Request*>::const_iterator i = nam_queue_sent.begin(); i != nam_queue_sent.end(); i++ )
    {
        if ( i.value()->api_command == api_command )
            times++;
    }

    return times >= min_times;
}

void BaseREST::removeRequest( const QString &api_command, const QString &body )
{
    QQueue<Request*> removed_requests;
    for ( QQueue<Request*>::const_iterator i = nam_queue.begin(); i != nam_queue.end(); i++ )
    {
        Request *const &req = *i;

        if ( req->api_command == api_command &&
             req->body == body )
            removed_requests.append( req );
    }

    // remove the requests we matched
    while ( removed_requests.size() > 0 )
        nam_queue.removeOne( removed_requests.takeFirst() );
}

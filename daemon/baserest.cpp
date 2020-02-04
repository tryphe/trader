#include "baserest.h"
#include "misctypes.h"
#include "global.h"
#include "engine.h"
#include "positionman.h"

#include <QTimer>
#include <QThread>

BaseREST::BaseREST( Engine *_engine )
    : QObject( nullptr ),
      engine( _engine )
{
    kDebug() << "[BaseREST]";

    // we use this to send the requests at a predictable rate
    send_timer = new QTimer( this );
    send_timer->setTimerType( Qt::CoarseTimer );

    // this timer checks for nam requests that have been queued too long
    timeout_timer = new QTimer( this );
    connect( timeout_timer, &QTimer::timeout, engine, &Engine::onCheckTimeouts );
    timeout_timer->setTimerType( Qt::VeryCoarseTimer );
    timeout_timer->start( 30000 );

    // this timer diverges/converges ping-pong orders
    diverge_converge_timer = new QTimer( this );
    connect( diverge_converge_timer, &QTimer::timeout, engine->getPositionMan(), &PositionMan::divergeConverge );
    diverge_converge_timer->setTimerType( Qt::VeryCoarseTimer );
    diverge_converge_timer->start( 100000 );

    // this timer reads the lo_sell and hi_buy prices for all coins
    ticker_timer = new QTimer( this );
    ticker_timer->setTimerType( Qt::VeryCoarseTimer );

    // this timer requests the order book
    orderbook_timer = new QTimer( this );
    orderbook_timer->setTimerType( Qt::VeryCoarseTimer );
}

BaseREST::~BaseREST()
{
    // make sure secret/key is gone
    keystore.clear();
    request_nonce = 0;

    // stop timers
    send_timer->stop();
    orderbook_timer->stop();
    timeout_timer->stop();
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

    delete send_timer;
    delete orderbook_timer;
    delete timeout_timer;
    delete diverge_converge_timer;
    delete ticker_timer;

    send_timer = nullptr;
    orderbook_timer = nullptr;
    timeout_timer = nullptr;
    diverge_converge_timer = nullptr;
    ticker_timer = nullptr;

    engine = nullptr;

    kDebug() << "[BaseREST] done.";
}

bool BaseREST::yieldToFlowControl() const
{
    return ( nam_queue.size() >= limit_commands_queued ||
             nam_queue_sent.size() >= limit_commands_sent );
}

bool BaseREST::yieldToServer( bool verbose ) const
{
    // stop sending commands if server is unresponsive
    if ( nam_queue_sent.size() > limit_commands_sent )
    {
        // print something every 2 mins
        static qint64 last_print_time = 0;
        const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
        if ( verbose && last_print_time < current_time - 120000 )
        {
            kDebug() << "local" << engine->engine_type << "info: nam_queue_sent > limit_commands_sent, waiting.";
            last_print_time = current_time;
        }

        return true;
    }

    return false;
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

bool BaseREST::isCommandQueued( const QString &api_command_prefix ) const
{
    // check for getopenorders request in nam_queue
    for ( QQueue<Request*>::const_iterator i = nam_queue.begin(); i != nam_queue.end(); i++ )
        if ( (*i)->api_command.startsWith( api_command_prefix ) )
            return true;

    return false;
}

bool BaseREST::isCommandSent( const QString &api_command_prefix, qint32 min_times ) const
{
    qint32 times = 0;

    // check for getopenorders request in nam_queue
    for ( QHash<QNetworkReply*,Request*>::const_iterator i = nam_queue_sent.begin(); i != nam_queue_sent.end(); i++ )
    {
        if ( (*i)->api_command.startsWith( api_command_prefix ) )
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

void BaseREST::deleteReply( QNetworkReply * const &reply, Request * const &request )
{
    // remove from tracking queue
    if ( reply == nullptr || request == nullptr )
    {
        kDebug() << "local error: got bad request/reply" << &request << &reply;
        return;
    }

    delete request;

    // if we took it out, it won't be in there. remove incase it's still there.
    nam_queue_sent.remove( reply );

    // send interrupt signal if we need to (if we are cleaning up replies in transit)
    if ( !reply->isFinished() )
        reply->abort();

    // delete from heap
    reply->deleteLater();
}

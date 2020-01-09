#include "bncrest.h"
#include "stats.h"
#include "engine.h"
#include "position.h"
#include "positionman.h"

#include <QTimer>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QWebSocket>
#include <QtMath>

BncREST::BncREST( Engine *_engine , QNetworkAccessManager *_nam )
  : BaseREST( _engine )
{
    kDebug() << "[BncREST]";

    nam = _nam;
    connect( nam, &QNetworkAccessManager::finished, this, &BncREST::onNamReply );

    exchange_string = BINANCE_EXCHANGE_STR;
}

BncREST::~BncREST()
{
    send_timer->stop();
    orderbook_timer->stop();
    exchangeinfo_timer->stop();
    ratelimit_timer->stop();

    delete send_timer;
    delete orderbook_timer;
    delete exchangeinfo_timer;
    delete ratelimit_timer;

    send_timer = nullptr;
    orderbook_timer = nullptr;
    exchangeinfo_timer = nullptr;
    ratelimit_timer = nullptr;

    kDebug() << "[BncREST] done.";
}

void BncREST::init()
{
    BaseREST::limit_commands_queued = 35; // stop checks if we are over this many commands queued
    BaseREST::limit_commands_queued_dc_check = 10; // exit dc check if we are over this many commands queued
    BaseREST::limit_commands_sent = 60; // stop checks if we are over this many commands sent
    BaseREST::limit_timeout_yield = 12;
    BaseREST::market_cancel_thresh = 300; // limit for market order total for weighting cancels to be sent first

    // we use this to send the requests at a predictable rate
    send_timer = new QTimer( this );
    connect( send_timer, &QTimer::timeout, this, &BncREST::sendNamQueue );
    send_timer->setTimerType( Qt::CoarseTimer );
    send_timer->start( BINANCE_TIMER_INTERVAL_NAM_SEND ); // minimum threshold 200 or so

    connect( ticker_timer, &QTimer::timeout, this, &BncREST::onCheckTicker );
    ticker_timer->start( BINANCE_TIMER_INTERVAL_TICKER );

    // this timer sets the network rate, fee, price ticksizes, and quantity ticksizes
    exchangeinfo_timer = new QTimer( this );
    connect( exchangeinfo_timer, &QTimer::timeout, this, &BncREST::onCheckExchangeInfo );
    exchangeinfo_timer->setTimerType( Qt::VeryCoarseTimer );
    exchangeinfo_timer->start( 60000 ); // 1 minute (turns to 1 hour after first parse)

    // this timer sets the network rate, fee, price ticksizes, and quantity ticksizes
    ratelimit_timer = new QTimer( this );
    connect( exchangeinfo_timer, &QTimer::timeout, this, &BncREST::onCheckRateLimit );
    ratelimit_timer->setTimerType( Qt::VeryCoarseTimer );
    ratelimit_timer->start( BINANCE_RATELIMIT_WINDOW ); // window is 1 minute

#if !defined( BINANCE_TICKER_ONLY )
    keystore.setKeys( BINANCE_KEY, BINANCE_SECRET );

    // this timer requests the order book
    orderbook_timer = new QTimer( this );
    connect( orderbook_timer, &QTimer::timeout, this, &BncREST::onCheckBotOrders );
    orderbook_timer->setTimerType( Qt::VeryCoarseTimer );
    orderbook_timer->start( BINANCE_TIMER_INTERVAL_ORDERBOOK );
    onCheckBotOrders();
#endif

    onCheckExchangeInfo();
    onCheckTicker();

    engine->loadSettings();
}

void BncREST::sendNamQueue()
{
    // check for requests
    if ( nam_queue.isEmpty() )
        return;

    // stop sending commands if server is unresponsive
    if ( nam_queue_sent.size() > limit_commands_sent )
    {
        // print something every minute
        static qint64 last_print_time = 0;
        qint64 current_time = QDateTime::currentMSecsSinceEpoch();
        if ( last_print_time < current_time - 60000 )
        {
            kDebug() << "local info: nam_queue_sent.size > limit_commands_sent, waiting.";
            last_print_time = current_time;
        }

        return;
    }

    // normalize ratelimit by our timer
    const qint32 ratelimit_window = ratelimit_minute;
    if ( binance_weight > ratelimit_window )
    {
        kDebug() << "local warning: hit ratelimit_window" << ratelimit_window;
        return;
    }

    QMultiMap<Coin/*weight*/,Request*> sorted_nam_queue;

    // insert into auto sorted map sorted by weight
    for ( QQueue<Request*>::const_iterator i = nam_queue.begin(); i != nam_queue.end(); i++ )
    {
        Request *const &request = *i;
        Position *const &pos = request->pos;

        // check for valid pos
        if ( !pos || !engine->getPositionMan()->isValid( pos ) )
        {
            sorted_nam_queue.insert( Coin(), request );
            continue;
        }

        // check for cancel
        if ( request->api_command == BNC_COMMAND_CANCEL &&
             engine->getPositionMan()->getMarketOrderTotal( pos->market ) >= market_cancel_thresh )
        {
            // expedite the cancel
            sorted_nam_queue.insert( CoinAmount::COIN, request );
            continue;
        }

        if ( request->api_command == BNC_COMMAND_GETTICKER ||
             request->api_command == BNC_COMMAND_GETORDERS )
        {
            sorted_nam_queue.insert( Coin(), request );
            continue;
        }

        // new hi/lo buy or sell, we should value this at 0 because it's not a reactive order
        if ( pos->is_new_hilo_order )
        {
            sorted_nam_queue.insert( Coin(), request );
            continue;
        }

        // check for not buy/sell command
        if ( request->api_command != BNC_COMMAND_BUYSELL )
        {
            sorted_nam_queue.insert( Coin(), request );
            continue;
        }

        sorted_nam_queue.insert( pos->per_trade_profit, request );
    }

    // go through orders, ranked from highest weight to lowest
    for ( QMultiMap<Coin,Request*>::const_iterator i = sorted_nam_queue.end() -1; i != sorted_nam_queue.begin() -1; i-- )
    {
        Request *const &request = i.value();

        // check if we received the orderbook in the timeframe of an order timeout grace period
        // if it's stale, we can assume the server is down and we let the orders timeout
        if ( yieldToLag() && request->api_command != BNC_COMMAND_GETORDERS )
        {
            // let this request hang around until the orderbook is responded to
            continue;
        }

        // track orders total and continue if we're over it
        if ( request->api_command.endsWith( "post-order" ) )
        {
            QString mdy_str = Global::getDateStringMDY();
            qint32 orders_today = daily_orders.value( mdy_str );

            if ( orders_today >= ratelimit_day )
            {
                kDebug() << "local warning: we are over the daily order ratelimit" << ratelimit_day;
                continue;
            }

            daily_orders[ mdy_str ]++;
        }

        sendNamRequest( request );
        // the request is added to sent_nam_queue and thus not deleted until the response is met
        return;
    }
}

void BncREST::sendNamRequest( Request *const &request )
{
    binance_weight += request->weight;

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    QString api_command = request->api_command;
    Position *const &pos = request->pos;

    // set the order request time because we are sending the request
    if ( api_command == BNC_COMMAND_BUYSELL &&
         engine->getPositionMan()->isQueued( pos ) )
    {
        pos->order_request_time = current_time;
    }
    // set cancel time properly
    else if ( api_command == BNC_COMMAND_CANCEL &&
              engine->getPositionMan()->isActive( pos ) )
    {
        pos->order_cancel_time = current_time;
    }

    // inherit the body from the input structure
    QUrlQuery query( request->body );

    // calculate a new nonce and compare it against the old nonce
    const qint64 request_nonce_new = current_time;
    QString request_nonce_str;

    // let a corrected nonce past incase we got a nonce error (allow multi-bot per key)
    if ( request_nonce_new <= request_nonce )
    {
        // let it past incase we overwrote with new_nonce
        request_nonce_str = QString::number( ++request_nonce );
    }
    else
    {
        request_nonce_str = QString::number( request_nonce_new );
        request_nonce = request_nonce_new;
    }

    // form nam request and body
    QNetworkRequest nam_request;
    QByteArray query_bytes;

    nam_request.setRawHeader( CONTENT_TYPE, CONTENT_TYPE_ARGS ); // add content header

    // add to sent queue so we can check if it timed out
    request->time_sent_ms = current_time;

    if ( api_command.startsWith( "sign-" ) )
    {
        api_command.remove( 0, 5 ); // remove "sign-" string

        query.addQueryItem( BNC_RECVWINDOW, "120000" ); // 2 minutes recvWindow because we aren't bad
        query.addQueryItem( BNC_TIMESTAMP, request_nonce_str );
        query.addQueryItem( BNC_SIGNATURE, Global::getBncSignature( query.toString().toUtf8(), keystore.getSecret() ) ); // add signature header

        // add signature to query
        query_bytes = query.toString().toUtf8();

        nam_request.setRawHeader( BNC_APIKEY, keystore.getKey() ); // add key header
    }

    QNetworkReply *reply = nullptr;
    // GET
    if ( api_command.startsWith( "get-" ) )
    {
        api_command.remove( 0, 4 ); // remove "get-"

        QString url_base = BNC_URL;

        // option to switch from v3 to v1
        if ( api_command.startsWith( "v1-" ) )
        {
            api_command.remove( 0, 3 );
            url_base.replace( "v3", "v1" );
        }

        QUrl public_url( url_base + api_command );

        public_url.setQuery( query );
        nam_request.setUrl( public_url );

        reply = nam->get( nam_request );
    }
    // POST
    else if ( api_command.startsWith( "post-" ) )
    {
        api_command.remove( 0, 5 ); // remove "post-"

        QUrl public_url( BNC_URL + api_command );

        nam_request.setUrl( public_url );
        reply = nam->post( nam_request, query_bytes );
    }
    // DELETE
    else if ( api_command.startsWith( "delete-" ) )
    {
        api_command.remove( 0, 7 ); // remove "delete-"

        QUrl public_url( BNC_URL + api_command );

        nam_request.setUrl( public_url );
        reply = nam->sendCustomRequest( nam_request, "DELETE", query_bytes );
    }

    if ( !reply )
    {
        kDebug() << "local error: failed to generate a valid QNetworkReply";
        return;
    }

    nam_queue_sent.insert( reply, request );
    nam_queue.removeOne( request );

    last_request_sent_ms = current_time;
}

void BncREST::sendBuySell( Position * const &pos, bool quiet )
{
    if ( !quiet )
        kDebug() << QString( "queued          %1" )
                    .arg( pos->stringifyOrderWithoutOrderID() );

    QUrlQuery query;
    query.addQueryItem( "symbol", pos->market.toExchangeString( ENGINE_BINANCE ) );
    query.addQueryItem( "side", pos->sideStr() );

    // set taker/maker order
    if ( pos->is_taker )
    {
        query.addQueryItem( "type", "LIMIT" );
        query.addQueryItem( "timeInForce", "GTC" );
    }
    else
    {
        query.addQueryItem( "type", "LIMIT_MAKER" );
    }

    query.addQueryItem( "price", pos->price );
    query.addQueryItem( "quantity", pos->quantity );

    // either post a buy or sell command
    sendRequest( BNC_COMMAND_BUYSELL, query.toString(), pos, 1 );
}

void BncREST::sendCancel( const QString &_order_id, Position *const &pos )
{
    // extract market from orderid (binance only)
    QString order_id = _order_id;
    QString market;
    while( order_id.size() > 0 && !order_id.at( 0 ).isDigit() )
    {
        market.append( order_id.left( 1 ) );
        order_id.remove( 0, 1 );
    }

    QUrlQuery query;
    query.addQueryItem( "symbol", market );
    query.addQueryItem( "orderId", order_id );

    sendRequest( BNC_COMMAND_CANCEL, query.toString(), pos, 1 );

    if ( pos && engine->getPositionMan()->isActive( pos ) )
    {
        pos->is_cancelling = true;

        // set the cancel time once here, and once on send, to avoid double cancel timeouts
        pos->order_cancel_time = QDateTime::currentMSecsSinceEpoch();
    }
}

void BncREST::sendGetOrder( const QString &_order_id, Position * const &pos )
{
    // extract market from orderid (binance only)
    QString order_id = _order_id;
    QString market;
    while( order_id.size() > 0 && !order_id.at( 0 ).isDigit() )
    {
        market.append( order_id.left( 1 ) );
        order_id.remove( 0, 1 );
    }

    QUrlQuery query;
    query.addQueryItem( "symbol", market );
    query.addQueryItem( "orderId", order_id );

    sendRequest( BNC_COMMAND_GETORDER, query.toString(), pos, 1 );
}

bool BncREST::yieldToLag() const
{
    const qint64 time = QDateTime::currentMSecsSinceEpoch();

    // have we seen the orderbook update recently?
    return ( orderbook_update_time != 0 &&
             orderbook_update_time < time - ( BINANCE_TIMER_INTERVAL_ORDERBOOK *5 ) );
}

void BncREST::onNamReply( QNetworkReply *const &reply )
{
    // don't process a reply we aren't tracking
    if ( !nam_queue_sent.contains( reply ) )
    {
        //kDebug() << "local warning: found stray response with no request object";
        //reply->deleteLater();
        return;
    }

    QByteArray data = reply->readAll();

    // reference the object we made during the request
    Request *const &request = nam_queue_sent.take( reply );
    const QString &api_command = request->api_command;
    const qint64 response_time = QDateTime::currentMSecsSinceEpoch() - request->time_sent_ms;

    avg_response_time.addResponseTime( response_time );

    //kDebug() << api_command << data;

    //kDebug() << "got reply for" << api_command;

    // parse any possible json in the body
    QJsonDocument body_json = QJsonDocument::fromJson( data );
    QJsonObject body_obj;
    QJsonArray body_arr;

    const bool is_body_object = body_json.isObject();
    const bool is_body_array = body_json.isArray();
    bool is_json_invalid = false;

    // detect what type of json we should parse
    if ( is_body_object )
        body_obj = body_json.object();
    else if ( is_body_array )
        body_arr = body_json.array();
    else
        is_json_invalid = true;

    // check for blank json
    if ( data.isEmpty() )
        is_json_invalid = true;

    // handle invalid json by resending certain commands
    if ( is_json_invalid )
    {
//        bool resent_command = false;
        const bool contains_html = data.contains( QByteArray( "<html" ) ) || data.contains( QByteArray( "<HTML" ) );

//        // resend cancel or on html data
//        if ( api_command != "returnOpenOrders" )
//        {
//            sendRequest( api_command, request->body, request->pos );
//            resent_command = true;
//        }
//        // resend buy/sell
//        else if ( api_command == BUY || api_command == SELL )
//        {
//            // check for bad ptr, and the position should also be queued
//            if ( !request->pos || !positions_queued.contains( request->pos ) )
//            {
//                deleteReply( reply, request );
//                return;
//            }

//            sendRequest( api_command, request->body, request->pos );
//            resent_command = true;
//        }

        // filter html to reduce spam
        if ( contains_html )
        {
            data = QByteArray( "<html error>" );
        }
        // filter malformed json
        else // we already know the json is invalid
        {
            //data = QByteArray( "<incomplete json>" );
        }

        // print the command and the invalid data
        kDebug() << QString( "nam error for %1: %2" )
                    .arg( api_command )
                    .arg( QString::fromLocal8Bit( data ) );

        deleteReply( reply, request );
        return;
    }

    if ( api_command == BNC_COMMAND_GETORDERS )
    {
        parseOpenOrders( body_arr, request->time_sent_ms );
    }
    else if ( api_command == BNC_COMMAND_GETTICKER )
    {
        parseTicker( body_arr, request->time_sent_ms );
    }
    else if ( api_command == BNC_COMMAND_GETEXCHANGEINFO )
    {
        parseExchangeInfo( body_obj );
    }
    else if ( api_command == BNC_COMMAND_BUYSELL )
    {
        parseBuySell( request, body_obj );
    }
    else if ( api_command == BNC_COMMAND_CANCEL )
    {
        parseCancelOrder( request, body_obj );
    }
    else if ( api_command == BNC_COMMAND_GETBALANCES )
    {
        parseReturnBalances( body_obj );
    }
    else
    {
        // parse unknown command
        kDebug() << "nam reply:" << api_command << data;
    }

    // cleanup
    deleteReply( reply, request );
}

void BncREST::onCheckBotOrders()
{
    // return on unset key/secret, or if we already queued this command
    if ( isKeyOrSecretUnset() || isCommandQueued( BNC_COMMAND_GETORDERS ) || isCommandSent( BNC_COMMAND_GETORDERS, 10 ) )
        return;

    // get list of open orders
    sendRequest( BNC_COMMAND_GETORDERS, "", nullptr, 40 );
}

void BncREST::onCheckTicker()
{
    // check if wss disconnected
    wssCheckConnection();

    if ( isCommandQueued( BNC_COMMAND_GETTICKER ) || isCommandSent( BNC_COMMAND_GETTICKER, 10 ) )
        return;

    sendRequest( BNC_COMMAND_GETTICKER, "", nullptr, 2 );
}

void BncREST::onCheckExchangeInfo()
{ // happens every hour

    if ( isCommandQueued( BNC_COMMAND_GETEXCHANGEINFO ) || isCommandSent( BNC_COMMAND_GETEXCHANGEINFO, 2 ) )
        return;

    sendRequest( BNC_COMMAND_GETEXCHANGEINFO, "", nullptr, 1 );
}

void BncREST::onCheckRateLimit()
{ // happens every minute
    binance_weight = 0;
}

void BncREST::wssConnected()
{
    wssSendSubscriptions();
}

void BncREST::wssSendSubscriptions()
{
}

void BncREST::wssSendJsonObj( const QJsonObject &obj )
{
    Q_UNUSED( obj )
}

void BncREST::wssCheckConnection()
{
}

void BncREST::wssTextMessageReceived( const QString &msg )
{
    Q_UNUSED( msg )
}

void BncREST::parseBuySell( Request *const &request, const QJsonObject &response )
{
    //kDebug();

    // check if we have a position recorded for this request
    if ( !request->pos )
    {
        kDebug() << "local error: found response for queued position, but postion is null";
        return;
    }

    // check that the position is queued and not set
    if ( !engine->getPositionMan()->isQueued( request->pos ) )
    {
        kDebug() << "local warning: position from response not found in positions_queued";
        return;
    }

    Position *const &pos = request->pos;

    // if we scan-set the order, it'll have an id. skip if the id is set
    // exiting here lets us have simultaneous scan-sets across different indices with the same prices/sizes
    if ( pos->order_number.size() > 0 )
        return;

    // look for post-only error
    if ( response[ "msg" ].toString() == "Order would immediately match and take." )
    {
        // try a better post-only price
        engine->findBetterPrice( pos );

        // resend command
        sendBuySell( pos );
        return;
    }

    if ( !response.contains( "orderId" ) )
    {
        kDebug() << "local error: tried to parse order but id was blank:" << response;
        return;
    }

    const QString &order_number = response[ "orderId" ].toVariant().toString(); // get the order number to track position id

    engine->getPositionMan()->activate( pos, order_number );
}

void BncREST::parseCancelOrder( Request *const &request, const QJsonObject &response )
{
    // request must be valid!

    Position *const &pos = request->pos;

    // prevent unsafe access
    if ( !pos || !engine->getPositionMan()->isActive( pos ) )
    {
        kDebug() << "successfully cancelled non-local order:" << response;
        return;
    }

    const QString &msg = response.value( "msg" ).toString();
    const bool error_unknown_order = ( msg == "Unknown order sent." );

    // handle error on manually cancelled order
    if ( error_unknown_order &&
         pos->cancel_reason == CANCELLING_FOR_USER )
    {
        engine->processCancelledOrder( pos );
        return;
    }

    // cancel-n-fill
    if ( error_unknown_order )
    {
        // single order fill
        sendGetOrder( pos->order_number, pos );
        return;
    }

    // check if it failed it some other way (we want it to complain)
    if ( response.value( "status" ).toString() != "CANCELED" )
    {
        kDebug() << "local error: cancel failed:" << response << "for pos" << pos->stringifyOrder();
        return;
    }

    kDebug() << "successfully cancelled order:" << response;

    engine->processCancelledOrder( pos );
}

void BncREST::parseOpenOrders( const QJsonArray &markets, qint64 request_time_sent_ms )
{
    //kDebug() << "got openOrders" << markets;

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch(); // cache time
    QVector<QString> order_numbers; // keep track of order numbers
    QMultiHash<QString, OrderInfo> orders;

    // is the orderbook is too old to be safe? check the stale tolerance
    if ( request_time_sent_ms < current_time - orderbook_stale_tolerance )
    {
        orders_stale_trip_count++;
        return;
    }

    // don't accept responses for requests sooner than the latest response request_time_sent_ms
    if ( request_time_sent_ms < orderbook_update_request_time )
        return;

    // set the timestamp of orderbook update if we saw any orders
    orderbook_update_time = current_time;
    orderbook_update_request_time = request_time_sent_ms;

    for ( QJsonArray::const_iterator i = markets.begin(); i != markets.end(); i++ )
    {
        // the first level is arrays of orders
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &order = (*i).toObject();
        const QString &market = order.value( "symbol" ).toString();
        const QString &order_number = market + order.value( "orderId" ).toVariant().toString();
        const QString &side = order.value( "side" ).toString().toLower();
        const Coin &price = order.value( "price" ).toString();
        const Coin &original_quantity = order.value( "origQty" ).toString();
        const Coin &btc_amount = price * original_quantity;

        //kDebug() << market << order_number << side << price << btc_amount;

        // check for missing information
        if ( market.isEmpty()||
             order_number.isEmpty() ||
             side.isEmpty() ||
             price.isZeroOrLess() ||
             btc_amount.isZeroOrLess() )
            continue;

        // insert into seen orders
        order_numbers += order_number;

        // insert (market, order)
        orders.insert( market, OrderInfo( order_number, side == BUY ? SIDE_BUY : side == SELL ? SIDE_SELL : 0, price, btc_amount ) );
    }

    engine->processOpenOrders( order_numbers, orders, request_time_sent_ms );
}

void BncREST::parseReturnBalances( const QJsonObject &obj )
{ // prints exchange balances
    Coin total_btc_value;

    //kDebug() << obj;

    if ( !obj[ "balances" ].isArray() )
    {
        kDebug() << "api error: couldn't parse balances:" << obj;
        return;
    }

    const QJsonArray &balances = obj[ "balances" ].toArray();

    for ( QJsonArray::const_iterator i = balances.begin(); i != balances.end(); i++ )
    {
        // parse object only
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &stats = (*i).toObject();

        const QString &currency = stats.value( "asset" ).toString();
        const    Coin available = stats.value( "free" ).toString();
        const    Coin onOrders = stats.value( "locked" ).toString();
        const    Coin total = available + onOrders;

        // skip zero balances
        if ( available.isZeroOrLess() &&
             onOrders.isZeroOrLess() )
            continue;

        Coin btcValue = total;
        if ( currency != "BTC" )
            btcValue *= engine->getPositionMan()->getHiBuy( Market( "BTC", currency ) );

        //kDebug() << currency << available << total << btcValue;

        kDebug() << QString( "%1 TOTAL: %2 AVAIL: %3 ORDER: %4 BTC_VAL: %5" )
                        .arg( currency, -8 )
                        .arg( total, -20 )
                        .arg( available, -20 )
                        .arg( onOrders, -20 )
                        .arg( btcValue, -20 );

        total_btc_value += btcValue;
    }

    kDebug() << "total btc value:" << total_btc_value;
}

void BncREST::parseTicker( const QJsonArray &info, qint64 request_time_sent_ms )
{
    //kDebug() << info;

    // check for data
    if ( info.isEmpty() )
        return;

    // if we don't have any market aliases loaded, skip for now (wait for getExchangeInfo)
    if ( market_aliases.isEmpty() )
        return;

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // is the orderbook is too old to be safe? check the stale tolerance
    if ( request_time_sent_ms < current_time - orderbook_stale_tolerance )
    {
        books_stale_trip_count++;
        return;
    }

    // don't accept responses for requests sooner than the latest response request_time_sent_ms
    if ( request_time_sent_ms < orderbook_public_update_request_time )
        return;

    orderbook_public_update_time = current_time;
    orderbook_public_update_request_time = request_time_sent_ms;

    // iterate through each market object
    QMap<QString, TickerInfo> ticker_info;

    for ( QJsonArray::const_iterator i = info.begin(); i != info.end(); i++ )
    {
        if ( !(*i).isObject() )
            continue;

        // read object and key
        const QJsonObject &market_obj = (*i).toObject();
        const QString &market_dirty = market_obj[ "symbol" ].toString();

        if ( !market_aliases.contains( market_dirty ) )
        {
            kDebug() << "local warning: couldn't find market alias for binance market" << market_dirty;
            continue;
        }

        const QString &market = market_aliases.value( market_dirty );

        if ( !market_obj.contains( "askPrice" ) ||
             !market_obj.contains( "bidPrice" ) )
            continue;

        // the object has two arrays of arrays [[1,2],[3,4]]
        const Coin ask_price = market_obj[ "askPrice" ].toString();
        const Coin bid_price = market_obj[ "bidPrice" ].toString();

        //kDebug() << market << bid_price << ask_price;

        // update our maps
        if ( market.size() > 0 &&
             bid_price.isGreaterThanZero() &&
             ask_price.isGreaterThanZero() )
        {
            ticker_info.insert( market, TickerInfo( bid_price, ask_price ) );
        }
    }

    engine->processTicker( this, ticker_info, request_time_sent_ms );
}

void BncREST::parseExchangeInfo( const QJsonObject &obj )
{
    const QJsonArray &rate_limits = obj[ "rateLimits" ].toArray();
    const QJsonArray &symbols = obj[ "symbols" ].toArray();

    for ( QJsonArray::const_iterator i = rate_limits.begin(); i != rate_limits.end(); i++ )
    {
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &ratelimit = (*i).toObject();

        //qDebug() << ratelimit;

        const QString &interval = ratelimit[ "interval" ].toString();
        const qint32 limit = ratelimit[ "limit" ].toInt();

        if ( interval == "MINUTE" && limit > 1 )
        {
            ratelimit_minute = qFloor( limit * 0.75 );
        }
        else if ( interval == "SECOND" && limit > 3 )
        {
            ratelimit_second = limit - 2; // be nice, subtract 2 from limit
            send_timer->setInterval( 1000 / ( ratelimit_second ) );
            //qDebug() << "send_timer interval set to" << send_timer->interval();
        }
        else if ( interval == "DAY" && limit > 101 )
        {
            ratelimit_day = limit - 100;
        }
    }

    for ( QJsonArray::const_iterator i = symbols.begin(); i != symbols.end(); i++ )
    {
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &symbol_info = (*i).toObject();
        const QString market = symbol_info[ "quoteAsset" ].toString() + QString( "_" ) + symbol_info[ "baseAsset" ].toString();
        const QString market_alias = symbol_info[ "baseAsset" ].toString() + symbol_info[ "quoteAsset" ].toString();

        // store the alias for this market later, because there's no separator
        market_aliases.insert( market_alias, market );

        const QJsonArray &filters = symbol_info[ "filters" ].toArray();

        if ( market_alias.isEmpty() || filters.isEmpty() )
            continue;

        // read market info
        MarketInfo &market_info = engine->getMarketInfo( market );

        for ( QJsonArray::const_iterator j = filters.begin(); j != filters.end(); j++ )
        {
            const QJsonObject &filter = (*j).toObject();

            const QString &filter_type = filter[ "filterType" ].toString();

            if ( filter_type == "PRICE_FILTER" )
            {
                const Coin &price_tick = filter[ "tickSize" ].toString();
                //qDebug() << "price ticksize" << price_tick;

                if ( price_tick.isGreaterThanZero() )
                    market_info.price_ticksize = price_tick;
                else
                    kDebug() << "local error: failed to parse 'tickSize'" << filter;
            }
            else if ( filter_type == "LOT_SIZE" )
            {
                const Coin &quantity_tick = filter[ "stepSize" ].toString();
                //qDebug() << "quantity ticksize" << quantity_tick;

                if ( quantity_tick.isGreaterThanZero() )
                    market_info.quantity_ticksize = quantity_tick;
                else
                    kDebug() << "local error: failed to parse 'stepSize'" << filter;
            }
            else if ( filter_type == "PERCENT_PRICE" )
            {
                const Coin &min = filter[ "multiplierDown" ].toString();
                const Coin &max = filter[ "multiplierUp" ].toString();
                //kDebug() << filter_type << market << Global::toSatoshiFormat( min ) << Global::toSatoshiFormat( max );

                if ( min.isGreaterThanZero() && max.isGreaterThanZero() )
                {
                    market_info.price_min_mul = min;
                    market_info.price_max_mul = max;
                }
                else
                    kDebug() << "local error: failed to parse 'multiplierDown' 'multiplierUp'" << filter;
            }
        }
    }

    // after we get a response, turn our timer interval up
    static const qint32 exchangeinfo_interval = 60000 * 60; // 1 hour
    if ( exchangeinfo_timer->interval() < exchangeinfo_interval )
        exchangeinfo_timer->setInterval( exchangeinfo_interval );
}

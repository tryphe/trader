#include "trexrest.h"

#if defined(EXCHANGE_BITTREX)

#include "position.h"
#include "positionman.h"
#include "stats.h"
#include "engine.h"
#include "keydefs.h"

#include <QTimer>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QWebSocket>
#include <QDebug>

TrexREST::TrexREST( Engine *_engine )
  : BaseREST( _engine )
{
    kDebug() << "[TrexREST]";
}

TrexREST::~TrexREST()
{
    order_history_timer->stop();
    delete order_history_timer;
    order_history_timer = nullptr;

    kDebug() << "[TrexREST] done.";
}

void TrexREST::init()
{
    BaseREST::limit_commands_queued = 20; // stop checks if we are over this many commands queued
    BaseREST::limit_commands_queued_dc_check = 7; // exit dc check if we are over this many commands queued
    BaseREST::limit_commands_sent = 45; // stop checks if we are over this many commands sent
    BaseREST::limit_timeout_yield = 6;
    BaseREST::market_cancel_thresh = 300; // limit for market order total for weighting cancels to be sent first

    keystore.setKeys( BITTREX_KEY, BITTREX_SECRET );

    connect( nam, &QNetworkAccessManager::finished, this, &TrexREST::onNamReply );

    // we use this to send the requests at a predictable rate
    send_timer = new QTimer( this );
    connect( send_timer, &QTimer::timeout, this, &TrexREST::sendNamQueue );
    send_timer->setTimerType( Qt::CoarseTimer );
    send_timer->start( 330 ); // recommended threshold 1s

    // this timer requests the order book
    order_history_timer = new QTimer( this );
    connect( order_history_timer, &QTimer::timeout, this, &TrexREST::onCheckOrderHistory );
    order_history_timer->setTimerType( Qt::VeryCoarseTimer );
    order_history_timer->start( 3000 );

    // this timer requests the order book
    orderbook_timer = new QTimer( this );
    connect( orderbook_timer, &QTimer::timeout, this, &TrexREST::onCheckBotOrders );
    orderbook_timer->setTimerType( Qt::VeryCoarseTimer );
    orderbook_timer->start( 20000 );

    // this timer reads the lo_sell and hi_buy prices for all coins
    ticker_timer = new QTimer( this );
    connect( ticker_timer, &QTimer::timeout, this, &TrexREST::onCheckOrderBooks );
    ticker_timer->setTimerType( Qt::VeryCoarseTimer );
    ticker_timer->start( 10000 );

#ifdef EXTRA_NICE
    order_history_timer->setInterval( 50000 );
    orderbook_timer->setInterval( 51000 );
    ticker_timer->setInterval( 52000 );
#endif

    onCheckBotOrders();
    onCheckOrderBooks();
}

void TrexREST::sendNamQueue()
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

    QMultiMap<Coin/*weight*/,Request*> sorted_nam_queue;

    // insert into auto sorted map sorted by weight
    for ( QQueue<Request*>::const_iterator i = nam_queue.begin(); i != nam_queue.end(); i++ )
    {
        Request *const &request = *i;
        Position *const &pos = request->pos;

        // check for valid pos
        if ( !pos || !engine->positions->isValid( pos ) )
        {
            sorted_nam_queue.insert( Coin(), request );
            continue;
        }

        // check if we should prioritize cancel
        if ( request->api_command == TREX_COMMAND_CANCEL &&
             engine->positions->getMarketOrderTotal( pos->market ) >= market_cancel_thresh )
        {
            // expedite the cancel
            sorted_nam_queue.insert( CoinAmount::COIN, request );
            continue;
        }

        if ( request->api_command == TREX_COMMAND_GET_ORDER_HIST )
        {
            sorted_nam_queue.insert( CoinAmount::SATOSHI, request );
            continue;
        }

        // new hi/lo buy or sell, we should value this at 0 because it's not a reactive order
        if ( pos->is_new_hilo_order )
        {
            sorted_nam_queue.insert( Coin(), request );
            continue;
        }

        // check for not buy/sell command
        if ( request->api_command != TREX_COMMAND_BUY &&
             request->api_command != TREX_COMMAND_SELL )
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
        if ( yieldToLag() && request->api_command != TREX_COMMAND_GET_ORDER_HIST )
        {
            // let this request hang around until the orderbook is responded to
            continue;
        }

        sendNamRequest( request );
        // the request is added to sent_nam_queue and thus not deleted until the response is met
        return;
    }
}

void TrexREST::sendNamRequest( Request *const &request )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    const QString &api_command = request->api_command;

    // set the order request time because we are sending the request
    if ( engine->positions->isQueued( request->pos ) &&
         ( api_command == TREX_COMMAND_BUY || api_command == TREX_COMMAND_SELL ) )
    {
        request->pos->order_request_time = current_time;
    }
    // set cancel time properly
    else if ( engine->positions->isActive( request->pos ) &&
              api_command == TREX_COMMAND_CANCEL )
    {
        request->pos->order_cancel_time = current_time;
    }

    // add to sent queue so we can check if it timed out
    request->time_sent_ms = current_time;

    // create url which will hold 'url'+'query_args'
    QUrl url = QUrl( TREX_REST_URL + api_command );

    // inherit the body from the input structure
    QUrlQuery query = QUrlQuery( request->body );

    const QString request_nonce_str = QString::number( current_time );

    query.addQueryItem( NONCE, request_nonce_str );
    query.addQueryItem( TREX_APIKEY, QString( keystore.getKey() ) );

    url.setQuery( query );

    QNetworkRequest nam_request;
    nam_request.setUrl( url );

    // add auth header
    nam_request.setRawHeader( TREX_APISIGN, Global::getBittrexPoloSignature( nam_request.url().toString().toLocal8Bit(), keystore.getSecret() ) );

    // send REST message
    QNetworkReply *const &reply = nam->get( nam_request );

    if ( !reply )
    {
        kDebug() << "local error: failed to generate a valid QNetworkReply";
        return;
    }

    nam_queue_sent.insert( reply, request );
    nam_queue.removeOne( request );
    last_request_sent_ms = current_time;
}

void TrexREST::sendBuySell( Position *const &pos, bool quiet )
{
    if ( !quiet )
        kDebug() << QString( "queued          %1" )
                    .arg( pos->stringifyOrderWithoutOrderID() );

    // serialize some request options into url format
    QUrlQuery query;
    query.addQueryItem( "market", pos->market.toExchangeString() );
    query.addQueryItem( "rate", pos->price );
    query.addQueryItem( "quantity", pos->quantity );

    // either post a buy or sell command
    sendRequest( "market/" + pos->sideStr() + "limit", query.toString(), pos );
}

void TrexREST::sendCancel( const QString &order_id, Position * const &pos )
{
    sendRequest( TREX_COMMAND_CANCEL, "uuid=" + order_id, pos );

    if ( pos && engine->positions->isActive( pos ) )
    {
        pos->is_cancelling = true;

        // set the cancel time once here, and once on send, to avoid double cancel timeouts
        pos->order_cancel_time = QDateTime::currentMSecsSinceEpoch();
    }
}

bool TrexREST::yieldToLag() const
{
    // have we seen the orderbook update recently?
    return ( order_history_update_time < QDateTime::currentMSecsSinceEpoch() - ( order_history_timer->interval() *10 ) /* ~50s */ );
}

void TrexREST::onNamReply( QNetworkReply *const &reply )
{
    // don't process a reply we aren't tracking
    if ( !nam_queue_sent.contains( reply ) )
    {
        // this will happen regularly for responses after a getorder
        //kDebug() << "local warning: found stray response with no request object";
        reply->deleteLater();
        return;
    }

    QString path = reply->url().path();
    QByteArray data = reply->readAll();

    // parse any possible json in the body
    QJsonDocument body_json = QJsonDocument::fromJson( data );
    QJsonObject body_obj = body_json.object();
    QJsonObject result_obj;
    QJsonArray result_arr;

    const QJsonValue &result_value = body_obj[ "result" ];
    bool success = body_obj[ "success" ].toBool();

    const bool is_array = result_value.isArray();
    const bool is_object = result_value.isObject();

    // cache result array/object
    if ( is_array )
        result_arr = result_value.toArray();
    else if ( is_object )
        result_obj = result_value.toObject();

    Request *const &request = nam_queue_sent.take( reply );
    const QString &api_command = request->api_command;
    const qint64 response_time = QDateTime::currentMSecsSinceEpoch() - request->time_sent_ms;

    avg_response_time.addResponseTime( response_time );

    // handle success=false
    if ( !success )
    {
        const QString &error_str = body_obj[ "message" ].toString();
        bool resent_command = false;
        bool is_html = data.contains( QByteArray( "<html" ) ) || data.contains( QByteArray( "<HTML" ) );
        bool is_throttled = error_str.endsWith( "Try again in 60 seconds." );

        // correctly delete orders we tried to cancel but are already filled/gone
        if ( api_command == TREX_COMMAND_CANCEL &&
             ( error_str == "ORDER_NOT_OPEN" || // detect cancel-n-fill #1 (this string was changed abruptly by bittrex to #2, we'll keep it)
               error_str == "INVALID_ORDER" ) ) // detect cancel-n-fill #2
        {
            Position *const &pos = request->pos;

            // prevent unallocated access (if we are cancelling it should be an active order)
            if ( !pos || !engine->positions->isActive( pos ) )
            {
                kDebug() << "unknown cancel reply:" << data;

                deleteReply( reply, request );
                return;
            }

            // cancel-n-fill
            if ( pos->cancel_reason == CANCELLING_FOR_SLIPPAGE_RESET || // we were cancelling an order with slippage that was filled
                 pos->cancel_reason == CANCELLING_FOR_DC )
            {
                // do single position fill
                engine->processFilledOrders( QVector<Position*>() << pos, FILL_CANCEL );
                deleteReply( reply, request );
                return;
            }

            // TODO: handle single positions that were cancelled in order to be onverged, but got filled in the meantime
            // 1) figure out which indices got filled and flip them with diverge_converge completes
            // 2) cancel the impending converge
            // 3) reset the orders that got cancelled

            // there are some errors that we don't handle yet, so we just go on with handling them as if everything is fine
            engine->processCancelledOrder( pos );

            deleteReply( reply, request );
            return;
        }
        // resend for all commands except common commands
        else if ( !is_throttled && // if we are throttled, let timeouts happen for regular commands
                  api_command != TREX_COMMAND_GET_ORDERS &&
                  api_command != TREX_COMMAND_GET_ORDER_HIST &&
                  api_command != TREX_COMMAND_GET_MARKET_SUMS &&
                  ( error_str == "" || error_str == "APIKEY_INVALID" || is_html ) )
        {
            // exceptions for buy/sell errors
            if ( api_command == TREX_COMMAND_BUY ||
                 api_command == TREX_COMMAND_SELL )
            {
                // check for bad ptr, and the position should also be queued
                if ( !request->pos || !engine->positions->isQueued( request->pos ) )
                {
                    deleteReply( reply, request );
                    return;
                }

                // don't re-send the request if we got this error after we set the order
                if ( request->pos->order_set_time > 0 )
                {
                    kDebug() << "local warning: avoiding re-sent request for order already set";
                    deleteReply( reply, request );
                    return;
                }
            }

            sendRequest( request->api_command, request->body, request->pos );
            resent_command = true;
        }

        // scan for html to reduce spam
        if ( is_html )
        {
            data = QByteArray( "<html error>" );
        }
        // scan for throttled command
        else if ( is_throttled )
        {
            data = QByteArray( "<throttled>" );
        }

        // print a nice message saying if we resent the command and print the invalid data
        kDebug() << QString( "%1nam error for %2: %3" )
                    .arg( resent_command ? "(resent) " : "" )
                    .arg( api_command )
                    .arg( QString::fromLocal8Bit( data ) );

        deleteReply( reply, request );
        return;
    }

    if ( api_command == TREX_COMMAND_GET_ORDER_HIST ) // history-fill
    {
        parseOrderHistory( body_obj );
    }
    else if ( api_command == TREX_COMMAND_GET_ORDERS ) // getorder-fill
    {
        parseOpenOrders( result_arr, request->time_sent_ms );
    }
    else if ( api_command == TREX_COMMAND_GET_MARKET_SUMS ) // ticker-fill
    {
        parseOrderBook( result_arr, request->time_sent_ms );
    }
    else if ( api_command == TREX_COMMAND_SELL || api_command == TREX_COMMAND_BUY )
    {
        parseBuySell( request, result_obj );
    }
    else if ( api_command == TREX_COMMAND_GET_BALANCES )
    {
        parseReturnBalances( result_arr );
    }
    else if ( api_command == TREX_COMMAND_GET_ORDER )
    {
        parseGetOrder( result_obj );
    }
    else if ( api_command == TREX_COMMAND_CANCEL )
    {
        parseCancelOrder( request, body_obj );
    }
    else
    {
        // parse unknown command
        kDebug() << "nam reply:" << api_command << data;
    }

    // cleanup
    deleteReply( reply, request );
}

void TrexREST::onCheckBotOrders()
{
    // return on unset key/secret, or if we already queued this command
    if ( isKeyOrSecretUnset() || isCommandQueued( TREX_COMMAND_GET_ORDERS ) || isCommandSent( TREX_COMMAND_GET_ORDERS, 10 ) )
        return;

    sendRequest( TREX_COMMAND_GET_ORDERS );
}

void TrexREST::onCheckOrderHistory()
{
    // ensure key/secret is set and command is not queued
    if ( isKeyOrSecretUnset() || isCommandQueued( TREX_COMMAND_GET_ORDER_HIST ) || isCommandSent( TREX_COMMAND_GET_ORDER_HIST, 10 ) )
        return;

    sendRequest( TREX_COMMAND_GET_ORDER_HIST );
}

void TrexREST::onCheckOrderBooks()
{
    if ( isCommandQueued( TREX_COMMAND_GET_MARKET_SUMS ) || isCommandSent( TREX_COMMAND_GET_MARKET_SUMS, 10 ) )
        return;

    sendRequest( TREX_COMMAND_GET_MARKET_SUMS );
}

void TrexREST::wssConnected()
{
}

void TrexREST::wssSendJsonObj( const QJsonObject &obj )
{
    Q_UNUSED( obj )
}

void TrexREST::wssCheckConnection()
{
}

void TrexREST::wssTextMessageReceived( const QString &msg )
{
    Q_UNUSED( msg )
}

void TrexREST::parseBuySell( Request *const &request, const QJsonObject &response )
{
    //kDebug();

    // check if we have a position recorded for this request
    if ( !request->pos )
    {
        kDebug() << "local error: found response for queued position, but postion is null";
        return;
    }

    // check that the position is queued and not set
    if ( !engine->positions->isQueued( request->pos ) )
    {
        //kDebug() << "local warning: position from response not found in positions_queued";
        return;
    }

    Position *const &pos = request->pos;

    // if we scan-set the order, it'll have an id. skip if the id is set
    // exiting here lets us have simultaneous scan-set across different indices with the same prices/sizes
    if ( pos->order_number.size() > 0 )
        return;

    if ( !response.contains( "uuid" ) )
    {
        kDebug() << "local warning: tried to parse order but id was blank:" << response;
        return;
    }

    const QString &order_number = response[ "uuid" ].toString(); // get the order number to track position id

    engine->positions->activate( pos, order_number );
}

void TrexREST::parseCancelOrder( Request *const &request, const QJsonObject &response )
{
    if ( !response.value( "success" ).toBool() )
    {
        // we shouldn't get here unless our code is bugged
        kDebug() << "local error: cancel failed:" << response;
        return;
    }

    Position *const &pos = request->pos;

    // prevent unsafe access
    if ( !pos || !engine->positions->isActive( pos ) )
    {
        kDebug() << "successfully cancelled non-local order:" << response;
        return;
    }

    engine->processCancelledOrder( pos );
}

void TrexREST::parseOpenOrders(const QJsonArray &orders, qint64 request_time_sent_ms )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch(); // cache time

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

    // parse array of objects
    QVector<QString> order_numbers; // keep track of order numbers
    QMultiHash<QString, OrderInfo> order_map; // store map of order numbers/orderinfo

    for ( QJsonArray::const_iterator i = orders.begin(); i != orders.end(); ++i )
    {
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &order = (*i).toObject();

        const QString &market = order.value( "Exchange" ).toString();
        const QString &order_number = order.value( "OrderUuid" ).toString();
        const QString &order_type = order.value( "OrderType" ).toString();
        const  quint8 &side = order_type == "LIMIT_BUY" ? SIDE_BUY :
                              order_type == "LIMIT_SELL" ? SIDE_SELL : 0;

        // the exchange uses a double (wtf), we'll read a double
        const Coin price = order.value( "Limit" ).toDouble();
        const Coin quantity = order.value( "Quantity" ).toDouble();
        const Coin btc_amount = quantity * price;

        //kDebug() << order;
        //kDebug() << market << side << btc_amount << "@" << price << order_number;

        // check for missing information
        if ( market.isEmpty() ||
             order_number.isEmpty() ||
             side == 0 ||
             btc_amount.isZeroOrLess() ) // only check btc_amount: if price or quantity is 0, btc_amount is also 0
            continue;

        // insert into seen orders
        order_numbers.append( order_number );

        // insert (market, order)
        order_map.insert( market, OrderInfo( order_number, side, price, btc_amount ) );
    }

    engine->processOpenOrders( order_numbers, order_map, request_time_sent_ms );
}

void TrexREST::parseReturnBalances( const QJsonArray &balances )
{ // prints exchange balances
    Coin total_d;

    for ( QJsonArray::const_iterator i = balances.begin(); i != balances.end(); ++i )
    {
        // parse object only
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &stats = (*i).toObject();
        //kDebug() << stats;

        const QString &currency = stats.value( "Currency" ).toString();
        const QString &address = stats.value( "CryptoAddress" ).toString();
        const Coin balance = stats.value( "Balance" ).toDouble();
        const Coin available = stats.value( "Available" ).toDouble();
        const Coin pending = stats.value( "Pending" ).toDouble();

        //kDebug() << available << balance;

        if ( balance.isZeroOrLess() && pending.isZeroOrLess() )
            continue;

        // store value of current currency
        Coin value_d = balance + pending;

        if ( currency == "BTC" ) // the price total is in btc, add it
        {
            total_d += value_d;
        }
        else // for alts, we format DOGE -> BTC-DOGE style string
        {
            value_d *= engine->positions->getHiBuy( Market( "BTC", currency ) );
            total_d += value_d;
        }

        QString out = QString( "%1: %2 AVAIL: %3 PEND: %4 VAL: %5" )
                       .arg( currency, -4 )
                       .arg( balance, -17 )
                       .arg( available, -17 )
                       .arg( pending, -16 )
                       .arg( value_d, -16 );

        // append deposit address if there is one
        if ( address.size() > 0 )
            out += " ADDR:" + address;

        kDebug() << out;
    }

    kDebug() << "total:" << total_d;
}

void TrexREST::parseGetOrder( const QJsonObject &order )
{
    // parse blank object correctly
    if ( !order.contains( "OrderUuid" ) ||
         !order.contains( "IsOpen" ) )
    {
        kDebug() << "local error: required fields were missing in getorder" << order;
        return;
    }

    const QString &order_id = order[ "OrderUuid" ].toString();
    const bool is_open = order[ "IsOpen" ].toBool();

    // debug output
    if ( is_open )
    {
        kDebug() << "nofill: order is still open, waiting for uuid" << order_id;
        return;
    }

    Position *const &pos = engine->positions->getByOrderID( order_id );

    if ( !pos )
        return;

    // do single order fill
    engine->processFilledOrders( QVector<Position*>() << pos, FILL_GETORDER );
}

void TrexREST::parseOrderBook( const QJsonArray &info, qint64 request_time_sent_ms )
{
    //kDebug() << "order book" << obj;

    // check for data
    if ( info.isEmpty() )
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

    QMap<QString, TickerInfo> ticker_info;

    // iterate through each market object
    for ( QJsonArray::const_iterator i = info.begin(); i != info.end(); i++ )
    {
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &info = (*i).toObject();

        //kDebug() << info;

        if ( !info.contains( "Ask" ) ||
             !info.contains( "Bid" ) )
            continue;

        const QString &market = info[ "MarketName" ].toString();
        const Coin ask = info[ "Ask" ].toDouble();
        const Coin bid = info[ "Bid" ].toDouble();

        //kDebug() << market << "bid:" << bid << "ask:" << ask;

        // update our maps
        if ( market.size() > 0 &&
             bid.isGreaterThanZero() &&
             ask.isGreaterThanZero() )
        {
            ticker_info.insert( market, TickerInfo( ask, bid ) );
        }
    }

    engine->processTicker( ticker_info, request_time_sent_ms );
}

void TrexREST::parseOrderHistory( const QJsonObject &obj )
{
    //kDebug() << "order history" << obj;

    const QJsonArray &orders = obj.value( "result" ).toArray();

    // check result size
    if ( orders.isEmpty() )
        return;

    order_history_update_time = QDateTime::currentMSecsSinceEpoch();

    QVector<Position*> filled_orders;

    for ( QJsonArray::const_iterator i = orders.begin(); i != orders.end(); i++ )
    {
        const QJsonObject &order = (*i).toObject();

        // make sure value exists and is zero
        if ( !order.contains( "OrderUuid" ) ||
             !order.contains( "QuantityRemaining" ) ||
              order.value( "QuantityRemaining" ).toDouble() != 0. )
            continue;

        const QString &order_id = order.value( "OrderUuid" ).toString();

        // make sure order number is valid
        Position *const &pos = engine->positions->getByOrderID( order_id );

        if ( order_id.isEmpty() || !pos )
            continue;

        // make sure not cancelling
        if ( pos->is_cancelling )
            continue;

        // add positions to process
        filled_orders += pos;
    }

    // process the orders
    engine->processFilledOrders( filled_orders, FILL_HISTORY );
}

#endif // EXCHANGE_BITTREX

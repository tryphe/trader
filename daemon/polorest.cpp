#include "polorest.h"
#include "position.h"
#include "positionman.h"
#include "engine.h"
#include "enginesettings.h"
#include "coinamount.h"

#include <QTimer>
#include <QNetworkAccessManager>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QThread>
#include <QWebSocket>

PoloREST::PoloREST( Engine *_engine , QNetworkAccessManager *_nam )
  : BaseREST( _engine )
{
    exchange_string = POLONIEX_EXCHANGE_STR;
    kDebug() << getExchangeFancyStr();

    nam = _nam;
    connect( nam, &QNetworkAccessManager::finished, this, &PoloREST::onNamReply );
}

PoloREST::~PoloREST()
{
#if !defined( POLONIEX_TICKER_ONLY )
    fee_timer->stop();
    wss_timer->stop();

    delete fee_timer;
    delete wss_timer;

    fee_timer = nullptr;
    wss_timer = nullptr;
#endif

    // dispose of websocket
    if ( wss )
    {
        // disconnect wss so we don't call wssCheckConnection()
        disconnect( wss, &QWebSocket::disconnected, this, &PoloREST::wssCheckConnection );

        wss->abort();
        delete wss;
        wss = nullptr;
    }

    kDebug() << getExchangeFancyStr() << "done.";
}

void PoloREST::init()
{
    BaseREST::limit_commands_queued = 28; // stop checks if we are over this many commands queued
    BaseREST::limit_commands_queued_dc_check = 10; // exit dc check if we are over this many commands queued
    BaseREST::limit_commands_sent = 48; // stop checks if we are over this many commands sent
    BaseREST::limit_timeout_yield = 5;
    BaseREST::market_cancel_thresh = 99; // limit for market order total for weighting cancels to be sent first

    // setup currency ids
    setupCurrencyMap( currency_name_by_id );

    // we use this to send the requests at a predictable rate
    connect( send_timer, &QTimer::timeout, this, &PoloREST::sendNamQueue );
    send_timer->start( POLONIEX_TIMER_INTERVAL_NAM_SEND ); // minimum threshold 200 or so

    connect( ticker_timer, &QTimer::timeout, this, &PoloREST::onCheckTicker );
    ticker_timer->start( POLONIEX_TIMER_INTERVAL_TICKER );

    // if we are running a ticker only build, don't set keys, don't get wss feed, and don't query books and fees
#if !defined( POLONIEX_TICKER_ONLY )
    keystore.setKeys( POLONIEX_KEY, POLONIEX_SECRET );

    // create websocket
    wss = new QWebSocket();
    connect( wss, &QWebSocket::connected, this, &PoloREST::wssConnected );
    connect( wss, &QWebSocket::disconnected, this, &PoloREST::wssCheckConnection );
    connect( wss, &QWebSocket::textMessageReceived, this, &PoloREST::wssTextMessageReceived );

    // this timer requests the order book
    connect( orderbook_timer, &QTimer::timeout, this, &PoloREST::onCheckBotOrders );
    orderbook_timer->start( POLONIEX_TIMER_INTERVAL_ORDERBOOK );

    // this timer syncs the maker fee so we can estimate profit
    fee_timer = new QTimer( this );
    connect( fee_timer, &QTimer::timeout, this, &PoloREST::onCheckFee );
    fee_timer->setTimerType( Qt::VeryCoarseTimer );
    fee_timer->start( 60000 * 60 * 12 ); // 12 hours (TODO: find the specific time that poloniex updates it)
    onCheckFee();

    // check websocket frequently
    wss_timer = new QTimer( this );
    connect( wss_timer, &QTimer::timeout, this, &PoloREST::wssCheckConnection );
    wss_timer->setTimerType( Qt::VeryCoarseTimer );
#endif

    onCheckTicker();

    engine->loadSettings();
}

bool PoloREST::yieldToLag() const
{
    const qint64 time = QDateTime::currentMSecsSinceEpoch();

    // have we seen the orderbook update recently?
    return ( orderbook_update_time != 0 &&
             orderbook_update_time < time - ( POLONIEX_TIMER_INTERVAL_ORDERBOOK *5 ) );
}

void PoloREST::sendNamRequest( Request *const &request )
{
    // check for valid pos
    if ( request->pos != nullptr && !engine->getPositionMan()->isValid( request->pos ) )
    {
        kDebug() << getExchangeFancyStr() << "local warning: caught nam request with invalid position";
        nam_queue.removeOne( request );
        return;
    }

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    const QString &api_command = request->api_command;
    Position *const &pos = request->pos;

    //  we are sending the request, set the order request time
    if ( ( api_command == BUY || api_command == SELL ) &&
         engine->getPositionMan()->isQueued( pos ) )
    {
        pos->order_request_time = current_time;
    }
    //  we are sending a cancel, set cancel time
    else if ( api_command == POLO_COMMAND_CANCEL &&
              engine->getPositionMan()->isActive( pos ) )
    {
        pos->order_cancel_time = current_time;
    }

    // inherit the body from the input structure
    QUrlQuery query( request->body );

    // add 'command' if it's not in the body yet
    if ( !query.hasQueryItem( COMMAND ) )
        query.addQueryItem( COMMAND, api_command );

    // don't set nonce unless api key is set
    if ( !keystore.isKeyOrSecretEmpty() )
        query.addQueryItem( NONCE, QString::number( ++request_nonce ) );

    // form nam request and body
    QNetworkRequest nam_request;
    const QByteArray query_bytes = query.toString().toLocal8Bit();

    // add content header
    nam_request.setRawHeader( CONTENT_TYPE, CONTENT_TYPE_ARGS );

    // incase we have POLONIEX_TICKER_ONLY enabled, don't sign a ticker request blank
    if ( request->api_command != POLO_COMMAND_GETBOOKS )
    {
        nam_request.setRawHeader( KEY, keystore.getKey() ); // add key header
        nam_request.setRawHeader( SIGN, Global::getBittrexPoloSignature( query_bytes, keystore.getSecret() ) ); // add signature header
    }

    // add to sent queue so we can check if it timed out
    request->time_sent_ms = current_time;

    QNetworkReply *reply;
    // GET
    if ( api_command == POLO_COMMAND_GETBOOKS )
    {
        // for a GET, we need to mash our query onto the end of the url instead of going into the body
        QString public_query = query.toString();
        //kDebug() << public_query;

        QUrl public_url( POLO_URL_PUBLIC );
        public_url.setQuery( query );
        nam_request.setUrl( public_url );

        reply = nam->get( nam_request );
    }
    // POST
    else
    {
        nam_request.setUrl( QUrl( POLO_URL_TRADE ) );
        reply = nam->post( nam_request, query_bytes );
    }

    if ( !reply )
    {
        kDebug() << getExchangeFancyStr() << "local error: failed to generate a valid QNetworkReply";
        return;
    }

    nam_queue_sent.insert( reply, request );
    nam_queue.removeOne( request );

    last_request_sent_ms = current_time;
}

void PoloREST::sendBuySell( Position *const &pos, bool quiet )
{
    if ( !quiet )
        kDebug() << QString( "queued          %1" )
                    .arg( pos->stringifyOrderWithoutOrderID() );

    // serialize some request options into url format
    QUrlQuery query;
    query.addQueryItem( "currencyPair", pos->market.toExchangeString( ENGINE_POLONIEX ) );
    query.addQueryItem( "rate", pos->price );
    query.addQueryItem( "amount", pos->quantity );

    // set post-only order
    if ( !pos->is_taker )
        query.addQueryItem( "postOnly", "1" );

    // either post a buy or sell command
    sendRequest( pos->sideStr(), query.toString(), pos );
}

void PoloREST::sendCancel( const QString &order_id, Position *const &pos )
{
    sendRequest( POLO_COMMAND_CANCEL, POLO_COMMAND_CANCEL_ARGS + order_id, pos );

    if ( pos && engine->getPositionMan()->isActive( pos ) )
    {
        pos->is_cancelling = true;

        // set the cancel time once here, and once on send, to avoid double cancel timeouts
        pos->order_cancel_time = QDateTime::currentMSecsSinceEpoch();
    }
}

void PoloREST::parseBuySell( Request *const &request, const QJsonObject &response )
{
    //kDebug() << response;

    // check if we have a position recorded for this request
    if ( !request->pos )
    {
        kDebug() << getExchangeFancyStr() << "local error: found response for queued position, but postion is null";
        return;
    }

    // check that the position is queued and not set
    if ( !engine->getPositionMan()->isQueued( request->pos ) )
    {
        //kDebug() << "local warning: position from response not found in positions_queued";
        return;
    }

    Position *const &pos = request->pos;

    // if we scan-set the order, it'll have an id. skip if the id is set
    // exiting here lets us have simultaneous scan-set across different indices with the same prices/sizes
    if ( !pos->order_number.isEmpty() )
        return;

    if ( !response.contains( "orderNumber" ) )
    {
        kDebug() << getExchangeFancyStr() << "local error: tried to parse order but id was blank:" << response;
        return;
    }

    const QString &order_number = response[ "orderNumber" ].toString(); // get the order number to track position id

    engine->getPositionMan()->activate( pos, order_number );
}

void PoloREST::parseCancelOrder( Request *const &request, const QJsonObject &response )
{
    // check if it succeeded, if so "success"=1
    if ( response.value( "success" ).toInt() != 1 )
    {
        kDebug() << getExchangeFancyStr() << "local error: cancel failed:" << response;
        return;
    }

    Position *const &pos = request->pos;

    // prevent unsafe access
    if ( !pos || !engine->getPositionMan()->isActive( pos ) )
    {
        kDebug() << getExchangeFancyStr() << "successfully cancelled non-local order:" << response;
        return;
    }

    engine->processCancelledOrder( pos );
}

void PoloREST::parseOpenOrders( const QJsonObject &markets, qint64 request_time_sent_ms )
{
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

    for ( QJsonObject::const_iterator i = markets.begin(); i != markets.end(); i++ )
    {
        // the first level is arrays of orders
        if ( !i.value().isArray() )
            continue;

        const QString &market = i.key();
        const QJsonArray &exchange_orders = i.value().toArray();

        for ( int j = 0; j < exchange_orders.size(); ++j )
        {
            // the second level is arrays of objects
            if ( !exchange_orders[j].isObject() )
                continue;

            const QJsonObject &order = exchange_orders[j].toObject();
            const QString &order_number = order.value( "orderNumber" ).toString();
            const QString &side = order.value( "type" ).toString();
            const QString &price = Coin( order.value( "rate" ).toString() ); // reformat into padded
            const QString &amount = Coin( order.value( "total" ).toString() ); // reformat into padded

            // check for missing information
            if ( market.isEmpty() ||
                 order_number.isEmpty() ||
                 side.isEmpty() ||
                 price.isEmpty() ||
                 amount.isEmpty() )
                continue;

            // insert into seen orders
            order_numbers.append( order_number );

            // insert (market, order)
            orders.insert( market, OrderInfo( order_number, side == BUY ? SIDE_BUY : side == SELL ? SIDE_SELL : 0, price, amount ) );
        }
    }

    engine->processOpenOrders( order_numbers, orders, request_time_sent_ms );
}

void PoloREST::parseReturnBalances( const QJsonObject &balances )
{ // prints exchange balances
    Coin total_btc_value;

    for ( QJsonObject::const_iterator i = balances.begin(); i != balances.end(); i++ )
    {
        // parse object only
        if ( !i.value().isObject() )
            continue;

        const QString &currency = i.key();
        const QJsonObject &stats = i.value().toObject();
        const Coin &btcValue = stats.value( "btcValue" ).toString();
        const Coin &available = stats.value( "available" ).toString();
        const Coin &onOrders = stats.value( "onOrders" ).toString();
        const Coin &total = available + onOrders;

        // skip zero balances
        if ( total.isZeroOrLess() )
            continue;

        QString out = QString( "%1 TOTAL: %2 AVAIL: %3 ORDER: %4 BTC_VAL: %5" )
                       .arg( currency, -8 )
                       .arg( total, -20 )
                       .arg( available, -20 )
                       .arg( onOrders, -20 )
                       .arg( btcValue, -20 );

        total_btc_value += btcValue;

        kDebug() << out;
    }

    kDebug() << "total btc value:" << total_btc_value;
}

void PoloREST::parseFeeInfo( const QJsonObject &info )
{
    // check for valid maker fee
    if ( !info.contains( "makerFee" ) )
    {
        kDebug() << "local warning: couldn't parse fee info:" << info;
        return;
    }

    const Coin maker = info[ "makerFee" ].toString();
    const Coin taker = info[ "takerFee" ].toString();
    const QString &thirty_day_volume = info[ "thirtyDayVolume" ].toString();

    if ( maker.isGreaterThanZero() )
    {
        engine->getSettings()->fee = maker;
        kDebug() << QString( "(fee) maker %1%, taker %2%, 30-day volume %3" )
                             .arg( ( maker * 100 ).toAmountString().mid( 0, 4 ) )
                             .arg( ( taker * 100 ).toAmountString().mid( 0, 4 ) )
                             .arg( thirty_day_volume );

        wss_timer->start( 30000 );
        wssCheckConnection(); // piggyback, submit wss stuff after first command sent
    }
    else
    {
        kDebug() << getExchangeFancyStr() << "local warning: fee was invalid:" << maker << taker << "fee info:" << info;
    }
}

void PoloREST::parseOrderBook( const QJsonObject &info, qint64 request_time_sent_ms )
{
    //kDebug() << info;

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
    if ( request_time_sent_ms < ticker_update_request_time )
        return;

    ticker_update_request_time = request_time_sent_ms;

    // iterate through each market object
    for ( QJsonObject::const_iterator i = info.begin(); i != info.end(); i++ )
    {
        if ( !i.value().isObject() )
            continue;

        // read object and key
        const QString &market = i.key();
        const QJsonObject &market_obj = i.value().toObject();

        if ( !market_obj.contains( "asks" ) ||
             !market_obj.contains( "bids" ) )
            continue;

        // the object has two arrays of arrays [[1,2],[3,4]]
        const QJsonArray &asks = market_obj[ "asks" ].toArray();
        const QJsonArray &bids = market_obj[ "bids" ].toArray();

        Coin hi_buy, lo_sell = CoinAmount::A_LOT;

        // walk asks
        for ( QJsonArray::const_iterator j = asks.begin(); j != asks.end(); j++ )
        {
            if ( !(*j).isArray() )
                continue;

            const QJsonArray asks_current = (*j).toArray();
            const Coin price = asks_current.at( 0 ).toString();

            if ( price < lo_sell )
                lo_sell = price;
        }

        // walk bids
        for ( QJsonArray::const_iterator j = bids.begin(); j != bids.end(); j++ )
        {
            if ( !(*j).isArray() )
                continue;

            const QJsonArray bids_current = (*j).toArray();
            const Coin price = bids_current.at( 0 ).toString(); // price is first item

            if ( price > hi_buy )
                hi_buy = price;
        }

//        kDebug() << market << "highest buy:" << toSatoshiFormat( hi_buy );
//        kDebug() << market << "lowest sell:" << toSatoshiFormat( lo_sell );

        // update our maps
        if ( !market.isEmpty() &&
             hi_buy.isGreaterThanZero() &&
             lo_sell < CoinAmount::A_LOT )
        {
            QMap<QString, Spread> ticker_info;
            ticker_info.insert( market, Spread( hi_buy, lo_sell ) );
            engine->processTicker( this, ticker_info, request_time_sent_ms );
        }
    }
}

void PoloREST::sendNamQueue()
{
    // check for requests
    if ( nam_queue.isEmpty() )
        return;

    // stop sending commands if server is unresponsive
    if ( yieldToFlowControl() )
        return;

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // we got a throttle error so wait until the throttle time
    if ( current_time < poloniex_throttle_time )
        return;

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
        if ( request->api_command == POLO_COMMAND_CANCEL &&
             engine->getPositionMan()->getMarketOrderTotal( pos->market ) >= market_cancel_thresh )
        {
            // expedite the cancel
            sorted_nam_queue.insert( CoinAmount::COIN, request );
            continue;
        }

        if ( request->api_command == POLO_COMMAND_GETBOOKS ||
             request->api_command == POLO_COMMAND_GETORDERS )
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
        if ( request->api_command != BUY &&
             request->api_command != SELL )
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
        if ( yieldToLag() && request->api_command != POLO_COMMAND_GETORDERS )
        {
            // let this request hang around until the orderbook is responded to
            continue;
        }

        sendNamRequest( request );
        // the request is added to sent_nam_queue and thus not deleted until the response is met
        return;
    }
}

void PoloREST::checkBotOrders( bool ignore_flow_control )
{
    // return on unset key/secret, or if we already queued this command
    if ( !ignore_flow_control && ( isKeyOrSecretUnset() || isCommandQueued( POLO_COMMAND_GETORDERS ) || isCommandSent( POLO_COMMAND_GETORDERS, 10 ) ) )
        return;

    sendRequest( POLO_COMMAND_GETORDERS, POLO_COMMAND_GETORDERS_ARGS );
}

void PoloREST::onCheckBotOrders()
{
    checkBotOrders();
}

void PoloREST::onCheckTicker()
{
    if ( isCommandQueued( POLO_COMMAND_GETBOOKS ) || isCommandSent( POLO_COMMAND_GETBOOKS ) )
        return;

    sendRequest( POLO_COMMAND_GETBOOKS, POLO_COMMAND_GETBOOKS_ARGS );
}

void PoloREST::onCheckFee()
{
    sendRequest( POLO_COMMAND_GETFEE );
}

void PoloREST::onNamReply( QNetworkReply *const &reply )
{
    // don't process a reply we aren't tracking
    if ( !nam_queue_sent.contains( reply ) )
        return;

    QByteArray data = reply->readAll();
    //kDebug() << data;

    // reference the object we made during the request
    Request *const &request = nam_queue_sent.take( reply );
    const QString &api_command = request->api_command;
    const qint64 response_time = QDateTime::currentMSecsSinceEpoch() - request->time_sent_ms;

    avg_response_time.addResponseTime( response_time );

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
    if ( body_arr.isEmpty() && body_obj.isEmpty() )
        is_json_invalid = true;

    // handle invalid json by resending certain commands
    if ( is_json_invalid )
    {
        bool resent_command = false;
        const bool contains_html = data.contains( QByteArray( "<html" ) ) || data.contains( QByteArray( "<HTML" ) );

        // resend cancel or on html data
        if ( api_command != POLO_COMMAND_GETORDERS && // skip spamming common commands
             api_command != POLO_COMMAND_GETBOOKS )
        {
            sendRequest( api_command, request->body, request->pos );
            resent_command = true;
        }
        // resend buy/sell
        else if ( api_command == BUY || api_command == SELL )
        {
            // check for bad ptr, and the position should also be queued
            if ( !request->pos || !engine->getPositionMan()->isQueued( request->pos ) )
            {
                deleteReply( reply, request );
                return;
            }

            sendRequest( api_command, request->body, request->pos );
            resent_command = true;
        }

        // filter html to reduce spam
        if ( contains_html )
        {
            data = QByteArray( "<html error>" );
        }
        // filter malformed json
        else // we already know the json is invalid
        {
            data = QByteArray( "<incomplete json>" );
        }

        // print a nice message saying if we resent the command and print the invalid data
        kDebug() << QString( "%1 %2nam error for %3: %4" )
                    .arg( getExchangeFancyStr() )
                    .arg( resent_command ? "(resent) " : "" )
                    .arg( api_command )
                    .arg( QString::fromLocal8Bit( data ) );

        deleteReply( reply, request );
        return;
    }

    // handle error messages by parsing the error string to resend certain commands
    if ( is_body_object && body_obj.contains( "error" ) )
    {
        bool resent_command = false;
        QString error_str = body_obj["error"].toString();

        if ( error_str.startsWith( "Please do not make more than 8" ) )
        {
            kDebug() << getExchangeFancyStr() << "throttling send queue for 3.5 seconds...";
            poloniex_throttle_time = QDateTime::currentMSecsSinceEpoch() + 3500;
        }

        // look for post-only error
        if ( ( api_command == BUY || api_command == SELL ) &&
             error_str.startsWith( "Unable to place post-only order at this price" ) )
        {
            Position *const &pos = request->pos;

            // prevent unallocated access (if we are cancelling it should be an active order)
            if ( !pos || !engine->getPositionMan()->isValid( pos ) ) // check positions_all, these should be queued not active
            {
                kDebug() << getExchangeFancyStr() << "unknown" << api_command << "reply:" << data;
                deleteReply( reply, request );
                return;
            }

            // try a better post-only price
            engine->findBetterPrice( pos );

            // resend command
            sendBuySell( pos );

            deleteReply( reply, request );
            return;
        }
        // correctly delete orders we tried to cancel but are already filled/gone
        else if ( api_command == POLO_COMMAND_CANCEL &&
             ( error_str.startsWith( "Invalid order number," ) ||
               error_str.endsWith( "is either completed or does not exist." ) ) )
        {
            Position *const &pos = request->pos;

            // prevent unallocated access (if we are cancelling it should be an active order)
            if ( !pos || !engine->getPositionMan()->isActive( pos ) )
            {
                kDebug() << getExchangeFancyStr() << "unknown cancel reply:" << data;
                deleteReply( reply, request );
                return;
            }

            // cancel-n-fill
            if ( pos->cancel_reason == CANCELLING_FOR_SLIPPAGE_RESET || // we were cancelling an order with slippage that was filled
                 pos->cancel_reason == CANCELLING_FOR_DC )
            {
                // do single order fill
                engine->processFilledOrders( QVector<Position*>() << pos, FILL_CANCEL );
                deleteReply( reply, request );
                return;
            }

            // TODO: handle single positions that were cancelled in order to be converged, but got filled in the meantime
            // 1) figure out which indices got filled and flip them with diverge_converge completes
            // 2) cancel the impending converge
            // 3) reset the orders that got cancelled

            // there are some errors that we don't handle yet, so we just go on with handling them as if everything is fine
            engine->processCancelledOrder( pos );

            deleteReply( reply, request );
            return;
        }
        // react to some common errors
        else if ( error_str.startsWith( "Nonce must be greater than" ) ||
             error_str.startsWith( "Transaction failed. Please try again." ) ||
             error_str.startsWith( "Please do not make more than 8" ) ||
             error_str.startsWith( "Order execution timed out." ) ||
             error_str.startsWith( "Internal error. Please try again." ) )
        {
            // resend buy/sell
            if ( api_command == BUY || api_command == SELL )
            {
                // check for bad ptr, and the position should also be queued
                if ( !request->pos || !engine->getPositionMan()->isQueued( request->pos ) )
                {
                    deleteReply( reply, request );
                    return;
                }

                // don't re-send the request if we got this error after we set the roder
                if ( request->pos->order_set_time > 0 )
                {
                    kDebug() << getExchangeFancyStr() << "local warning: avoiding re-sent request for order already set";
                    deleteReply( reply, request );
                    return;
                }

                sendRequest( api_command, request->body, request->pos );
                resent_command = true;
            }
            // resend other commands
            else if ( api_command == POLO_COMMAND_CANCEL ||
                 api_command == POLO_COMMAND_GETFEE ||
                 api_command == POLO_COMMAND_GETCHARTDATA )
            {
                sendRequest( api_command, request->body, request->pos );
                resent_command = true;
            }
        }
        // nonce error
        if ( error_str.startsWith( "Nonce must be great" ) )
        {
            // read the nonce from the message
            const QStringList words = error_str.split( QChar( ' ' ) );
            bool ok = false;
            qint64 new_nonce = words.value( 5 ).remove( QChar( '.' ) ).toLongLong( &ok ) +1;

            // make sure our local nonce is older than the new calculated nonce
            if ( ok == true && request_nonce < new_nonce )
            {
                kDebug() << getExchangeFancyStr() << "local info: nonce adjusted from" << request_nonce << "to" << new_nonce;
                request_nonce = new_nonce;
            }
            // if conversion failed, leave an error message
            else if ( ok == false )
            {
                kDebug() << getExchangeFancyStr() << "local error: got nonce error but conversion failed";
            }
            // if the nonce was old, leave an info message
            else
            {
                kDebug() << getExchangeFancyStr() << "local info: got nonce error but new nonce" << new_nonce << "is older than local nonce" << request_nonce;
            }

            if ( api_command == POLO_COMMAND_GETORDERS )
            {
                deleteReply( reply, request );
                return;
            }
            else
            {
                error_str = "<nonce error>";
            }
        }

        // don't print maintenance mode errors
        if ( error_str != "Currently in maintenance mode." )
            // print a nice message saying if we resent the command and print the invalid data
            kDebug() << QString( "%1 %2nam json error for %3: %4" )
                        .arg( getExchangeFancyStr() )
                        .arg( resent_command ? "(resent) " : "" )
                        .arg( api_command )
                        .arg( error_str );

        deleteReply( reply, request );
        return;
    }

    if ( api_command == POLO_COMMAND_GETORDERS )
    {
        parseOpenOrders( body_obj, request->time_sent_ms );
    }
    else if ( api_command == POLO_COMMAND_GETBOOKS )
    {
        parseOrderBook( body_obj, request->time_sent_ms );
    }
    else if ( api_command == BUY || api_command == SELL )
    {
        parseBuySell( request, body_obj );
    }
    else if ( api_command == POLO_COMMAND_CANCEL )
    {
        parseCancelOrder( request, body_obj );
    }
    else if ( api_command == POLO_COMMAND_GETBALANCES )
    {
        parseReturnBalances( body_obj );
    }
    else if ( api_command == POLO_COMMAND_GETFEE )
    {
        parseFeeInfo( body_obj );
    }
    else
    {
        // parse unknown command
        kDebug() << getExchangeFancyStr() << "unknown nam reply:" << api_command << data;
    }

    // cleanup
    deleteReply( reply, request );
}

void PoloREST::wssConnected()
{
    wssSendSubscriptions();
}

void PoloREST::wssSendSubscriptions()
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();

    // subscribe to account feed
    if ( !wss_1000_state &&
         wss_1000_subscribe_try_time < current_time - 30000 )
    {
        const QString nonce_payload = QString( "nonce=%1" ).arg( ++request_nonce );
        const QString sign = Global::getBittrexPoloSignature( nonce_payload.toUtf8(), keystore.getSecret() );

        const QJsonObject subscribe_account_notifications
        {
            { "command", "subscribe" },
            { "channel", 1000 },
            { "key", QString( keystore.getKey() ) },
            { "payload", nonce_payload },
            { "sign", sign }
        };

        kDebug() << getExchangeFancyStr() << "(wss) sending 1000 subscribe";
        wssSendJsonObj( subscribe_account_notifications );

        wss_1000_subscribe_try_time = current_time;
    }

    // subscribe to price feed
    if ( !wss_1002_state &&
         wss_1002_subscribe_try_time < current_time - 30000 )
    {
        const QJsonObject subscribe_1002
        {
            { "command", "subscribe" },
            { "channel", 1002 }
        };

        kDebug() << getExchangeFancyStr() << "(wss) sending 1002 subscribe";
        wssSendJsonObj( subscribe_1002 );

        wss_1002_subscribe_try_time = current_time;
    }
}

void PoloREST::wssSendJsonObj( const QJsonObject &obj )
{
    const QJsonDocument doc = QJsonDocument( obj );
    const QString data_str = doc.toJson( QJsonDocument::Compact );

    //kDebug() << "(wss) sending" << data_str;

    wss->sendTextMessage( data_str );
}

void PoloREST::setupCurrencyMap( QMap<qint32, QString> &m )
{
    // dumped from https://poloniex.com/support/api/

    m[ 7 ] = "BTC_BCN";
    m[ 8 ] = "BTC_BELA";
    m[ 10 ] = "BTC_BLK";
    m[ 12 ] = "BTC_BTCD";
    m[ 13 ] = "BTC_BTM";
    m[ 14 ] = "BTC_BTS";
    m[ 15 ] = "BTC_BURST";
    m[ 20 ] = "BTC_CLAM";
    m[ 24 ] = "BTC_DASH";
    m[ 25 ] = "BTC_DGB";
    m[ 27 ] = "BTC_DOGE";
    m[ 28 ] = "BTC_EMC2";
    m[ 31 ] = "BTC_FLDC";
    m[ 32 ] = "BTC_FLO";
    m[ 38 ] = "BTC_GAME";
    m[ 40 ] = "BTC_GRC";
    m[ 43 ] = "BTC_HUC";
    m[ 50 ] = "BTC_LTC";
    m[ 51 ] = "BTC_MAID";
    m[ 58 ] = "BTC_OMNI";
    m[ 61 ] = "BTC_NAV";
    m[ 63 ] = "BTC_NEOS";
    m[ 64 ] = "BTC_NMC";
    m[ 69 ] = "BTC_NXT";
    m[ 73 ] = "BTC_PINK";
    m[ 74 ] = "BTC_POT";
    m[ 75 ] = "BTC_PPC";
    m[ 83 ] = "BTC_RIC";
    m[ 89 ] = "BTC_STR";
    m[ 92 ] = "BTC_SYS";
    m[ 97 ] = "BTC_VIA";
    m[ 98 ] = "BTC_XVC";
    m[ 99 ] = "BTC_VRC";
    m[ 100 ] = "BTC_VTC";
    m[ 104 ] = "BTC_XBC";
    m[ 108 ] = "BTC_XCP";
    m[ 112 ] = "BTC_XEM";
    m[ 114 ] = "BTC_XMR";
    m[ 116 ] = "BTC_XPM";
    m[ 117 ] = "BTC_XRP";
    m[ 121 ] = "USDT_BTC";
    m[ 122 ] = "USDT_DASH";
    m[ 123 ] = "USDT_LTC";
    m[ 124 ] = "USDT_NXT";
    m[ 125 ] = "USDT_STR";
    m[ 126 ] = "USDT_XMR";
    m[ 127 ] = "USDT_XRP";
    m[ 129 ] = "XMR_BCN";
    m[ 130 ] = "XMR_BLK";
    m[ 131 ] = "XMR_BTCD";
    m[ 132 ] = "XMR_DASH";
    m[ 137 ] = "XMR_LTC";
    m[ 138 ] = "XMR_MAID";
    m[ 140 ] = "XMR_NXT";
    m[ 148 ] = "BTC_ETH";
    m[ 149 ] = "USDT_ETH";
    m[ 150 ] = "BTC_SC";
    m[ 151 ] = "BTC_BCY";
    m[ 153 ] = "BTC_EXP";
    m[ 155 ] = "BTC_FCT";
    m[ 158 ] = "BTC_RADS";
    m[ 160 ] = "BTC_AMP";
    m[ 162 ] = "BTC_DCR";
    m[ 163 ] = "BTC_LSK";
    m[ 166 ] = "ETH_LSK";
    m[ 167 ] = "BTC_LBC";
    m[ 168 ] = "BTC_STEEM";
    m[ 169 ] = "ETH_STEEM";
    m[ 170 ] = "BTC_SBD";
    m[ 171 ] = "BTC_ETC";
    m[ 172 ] = "ETH_ETC";
    m[ 173 ] = "USDT_ETC";
    m[ 174 ] = "BTC_REP";
    m[ 175 ] = "USDT_REP";
    m[ 176 ] = "ETH_REP";
    m[ 177 ] = "BTC_ARDR";
    m[ 178 ] = "BTC_ZEC";
    m[ 179 ] = "ETH_ZEC";
    m[ 180 ] = "USDT_ZEC";
    m[ 181 ] = "XMR_ZEC";
    m[ 182 ] = "BTC_STRAT";
    m[ 183 ] = "BTC_NXC";
    m[ 184 ] = "BTC_PASC";
    m[ 185 ] = "BTC_GNT";
    m[ 186 ] = "ETH_GNT";
    m[ 187 ] = "BTC_GNO";
    m[ 188 ] = "ETH_GNO";
    m[ 189 ] = "BTC_BCH";
    m[ 190 ] = "ETH_BCH";
    m[ 191 ] = "USDT_BCH";
    m[ 192 ] = "BTC_ZRX";
    m[ 193 ] = "ETH_ZRX";
    m[ 194 ] = "BTC_CVC";
    m[ 195 ] = "ETH_CVC";
    m[ 196 ] = "BTC_OMG";
    m[ 197 ] = "ETH_OMG";
    m[ 198 ] = "BTC_GAS";
    m[ 199 ] = "ETH_GAS";
    m[ 200 ] = "BTC_STORJ";
    m[ 201 ] = "BTC_EOS";
    m[ 202 ] = "ETH_EOS";
    m[ 203 ] = "USDT_EOS";
}

void PoloREST::wssCheckConnection()
{
    if ( !wss )
        return;

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    const qint64 wss_timeout = 30000;

    // check for connected
    if ( ( !wss->isValid() ||  // socket is invalid OR
           wss_heartbeat_time < current_time - wss_timeout ) && // we stopped receiving data
         wss_connect_try_time < current_time - wss_timeout && // last time we tried to connect is stale
         !yieldToLag() ) // make sure orderbook is still updating
    {
        kDebug() << "(wss-reconnect)";

        // update the time now incase open() is blocking when another disconnected() event fires
        wss_connect_try_time = current_time;
        wss_1000_state = false;
        wss_1002_state = false;
        wss_1000_subscribe_try_time = 0;
        wss_1002_subscribe_try_time = 0;

        wss->abort();
        wss->open( QUrl( POLO_URL_WSS ) );
    }
    else
    {
        // if we are connected, make sure feeds are active
        wssSendSubscriptions();
    }

    const bool wss_account_feed_is_up_to_date = wss_account_feed_update_time > current_time - wss_timeout;

    // websocket feed is up to date, adjust timer intervals to be slower
    if ( wss_account_feed_is_up_to_date &&
         orderbook_timer->interval() < 120000 )
    {
        orderbook_timer->setInterval( 120000 );
        ticker_timer->setInterval( 120000 );

        kDebug() << getExchangeFancyStr() << "(wss) slowed down orderbook timer and price timer";
    }
    // feed is old, restore intervals
    else if ( !wss_account_feed_is_up_to_date &&
              orderbook_timer->interval() > 5000 )
    {
        orderbook_timer->setInterval( 5000 );
        ticker_timer->setInterval( 10000 );

        kDebug() << getExchangeFancyStr() << "(wss) sped up orderbook timer and price timer";
    }
}

void PoloREST::wssTextMessageReceived( const QString &msg )
{
    static QJsonDocument doc;
    doc = QJsonDocument::fromJson( msg.toLocal8Bit() );

    //kDebug() << "wss in:" << msg;

    // check for array
    if ( !doc.isArray() )
        return;

    // parse the outer array
    // [1002,null,[179,0.44761404,0.44971726,0.44761404,-0.01142671,363.24909103,820.07867271,0,0.45800334,0.42984444]])
    static QJsonArray arr;
    arr = doc.array();

    const qint32 message_type = arr.at( 0 ).toVariant().toInt(); // occasionally, this is a string
    //message_type = message_type > 0 ? message_type : arr.at( 0 ).toString().toInt();

    const qint32 status = arr.at( 1 ).toInt();

    // update heartbeat time for any message
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    wss_heartbeat_time = current_time;

    // update our time that controls timer intervals
    if ( wss_1000_state )
        wss_account_feed_update_time = current_time;

    // check for heartbeat
    if ( message_type == 1010 )
        return;

    //kDebug() << message_type <<  arr.at( 1 ).toInt() ;

    if ( message_type == 1000 && status > 0 )
    {
        kDebug() << getExchangeFancyStr() << "(wss) 1000 account feed active";
        wss_1000_state = true;
        return;
    }

    if ( message_type == 1002 && status > 0 )
    {
        kDebug() << getExchangeFancyStr() << "(wss) 1002 price feed active";
        wss_1002_state = true;
        return;
    }

    // check for account info data
    if ( message_type == 1000 && arr.at( 2 ).isArray() )
    {
        //kDebug() << "wss-1000 in:" << msg;

        QVector<Position*> filled_orders;
        const QJsonArray &updates = arr.at( 2 ).toArray();

        for ( QJsonArray::const_iterator i = updates.begin(); i != updates.end(); i++ )
        {
            //kDebug() << (*i);
            if ( !(*i).isArray() )
                continue;

            const QJsonArray &info = (*i).toArray();

            // look for order fill
            if ( info.at( 0 ).toString() == "o" &&
                 info.at( 2 ).toString() == "0.00000000" )
            {
                const QString &order_id = info.at( 1 ).toVariant().toString();

                //kDebug() << "order filled or cancelled:" << order_id;

                // make sure order number is valid
                if ( order_id.isEmpty() )
                    continue;

                Position *const &pos = engine->getPositionMan()->getByOrderID( order_id );

                // make sure pos is valid
                if ( !pos )
                    continue;

                if ( pos->is_cancelling )
                    continue;

                // add order ids to process
                filled_orders += pos;
            }
        }

        // process the orders
        engine->processFilledOrders( filled_orders, FILL_WSS );

        return;
    }

    // check for non-ticker message
    if ( message_type == 1002 && arr.at( 2 ).isArray() )
    {
        // parse the inner array
        const QJsonArray &data = arr.at( 2 ).toArray();

        // data format from polo documentation:
        // [ <currency pair id>, "<last trade price>", "<lowest ask>", "<highest bid>",
        // "<percent change in last 24 hours>", "<base currency volume in last 24 hours>",
        // "<quote currency volume in last 24 hours>", <is frozen>, "<highest trade price in last 24 hours>",
        // "<lowest trade price in last 24 hours>" ]

        //kDebug() << "bid_str:" << data.at( 3 ).toString() << "ask_str:" << data.at( 2 ).toString();

        const qint32 currency_pair = data.at( 0 ).toInt();
        const QString &market = currency_name_by_id.value( currency_pair );
        const Coin ask = data.at( 2 ).toString();
        const Coin bid = data.at( 3 ).toString();

        //kDebug() << bid << ask;

        if ( !market.isEmpty() &&
             bid.isGreaterThanZero() &&
             ask.isGreaterThanZero() )
        {
            QMap<QString, Spread> ticker_info;
            ticker_info.insert( market, Spread( bid, ask ) );
            engine->processTicker( this, ticker_info );
        }
        return;
    }

    // print unhandled message
    kDebug() << getExchangeFancyStr() << "unhandled wss:" << msg;
}

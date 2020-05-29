#include "wavesrest.h"
#include "position.h"
#include "positionman.h"
#include "alphatracker.h"
#include "engine.h"
#include "wavesaccount.h"
#include "wavesutil.h"
#include "enginesettings.h"

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
#include <QDateTime>

static const int MAX_NEW_ORDERS_IN_FLIGHT = 2;

WavesREST::WavesREST( Engine *_engine, QNetworkAccessManager *_nam )
    : BaseREST( _engine )
{
    kDebug() << "[WavesREST]";

    nam = _nam;
    connect( nam, &QNetworkAccessManager::finished, this, &WavesREST::onNamReply );

    exchange_string = WAVES_EXCHANGE_STR;
}

WavesREST::~WavesREST()
{
    market_data_timer->stop();

    delete market_data_timer;
    market_data_timer = nullptr;

    kDebug() << "[WavesREST] done.";
}

void WavesREST::init()
{
    BaseREST::limit_commands_queued = 30; // stop checks if we are over this many commands queued
    BaseREST::limit_commands_sent = 10; // stop checks if we are over this many commands sent
    engine->getSettings()->order_timeout = 60000 * 15; // extend order timeout (we don't want stray orders during ddos)

    // init asset maps
    account.initAssetMaps();

    // we use this to send the requests at a predictable rate
    connect( send_timer, &QTimer::timeout, this, &WavesREST::sendNamQueue );
    send_timer->start( WAVES_TIMER_INTERVAL_NAM_SEND ); // minimum threshold 200 or so

    // this timer requests market data
    market_data_timer = new QTimer( this );
    market_data_timer->setTimerType( Qt::VeryCoarseTimer );
    connect( market_data_timer, &QTimer::timeout, this, &WavesREST::onCheckMarketData );
    market_data_timer->start( WAVES_TIMER_INTERVAL_MARKET_DATA );

    connect( ticker_timer, &QTimer::timeout, this, &WavesREST::onCheckTicker );
    ticker_timer->start( WAVES_TIMER_INTERVAL_TICKER );

#if !defined( WAVES_TICKER_ONLY )
    account.setPrivateKeyB58( WAVES_SECRET );

    // when ticker mode is disabled, set dummy keys so BaseREST::isKeyOrSecretUnset() returns a sane value
    keystore.setKeys( "dummy", "dummy" );

    connect( orderbook_timer, &QTimer::timeout, this, &WavesREST::onCheckBotOrders );
    orderbook_timer->start( WAVES_TIMER_INTERVAL_CHECK_MY_ORDERS );
#endif

    onCheckMarketData();

    engine->loadSettings();
}

void WavesREST::sendNamQueue()
{
    // stop sending commands if server is unresponsive
    if ( yieldToServer() )
        return;

    // check for orders that we should poll
    if ( nam_queue.isEmpty() )
    {
        while ( engine->orders_for_polling.size() > 0 )
        {
            const QString order_number = engine->orders_for_polling.takeFirst();

            // if it's invalid, just toss it and goto the next id
            if ( !engine->getPositionMan()->isValidOrderID( order_number ) )
                continue;

            getOrderStatus( engine->getPositionMan()->getByOrderID( order_number ) );
            break;
        }
    }

    // optimistically query cancelling/cancelled orders
    if ( nam_queue.isEmpty() )
        onCheckCancellingOrders();

    // check for no requests
    if ( nam_queue.isEmpty() )
        return;

    // go through requests
    for ( QQueue<Request*>::const_iterator i = nam_queue.begin(); i != nam_queue.end(); i++ )
    {
        Request *const &request = *i;

        // if 2 or more new order commands are in flight, wait for them
        if ( request->api_command.startsWith( "on-" ) && isCommandSent( "on-", MAX_NEW_ORDERS_IN_FLIGHT ) )
        {
            // print something every 2 mins
            static qint64 last_print_time = 0;
            const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
            if ( last_print_time < current_time - 120000 )
            {
                //kDebug() << "local" << engine->engine_type << "info: too many new orders in flight, waiting.";
                last_print_time = current_time;
            }

            continue;
        }

        sendNamRequest( request );
        // the request is added to sent_nam_queue and thus not deleted until the response is met
        return;
    }
}

void WavesREST::sendNamRequest( Request * const &request )
{
    // check for valid pos
    if ( request->pos != nullptr && !engine->getPositionMan()->isValid( request->pos ) )
    {
        kDebug() << "local warning: caught nam request with invalid position";
        nam_queue.removeOne( request );
        return;
    }

    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    QString api_command = request->api_command;

    request_nonce++; // bump nonce for baserest stats

    bool is_my_orders_request = false;

    // set the order request time for new order
    if ( api_command.startsWith( "on" ) )
        request->pos->order_request_time = current_time;
    // set cancel time properly
    else if ( api_command.startsWith( "oc" ) && request->pos != nullptr )
        request->pos->order_cancel_time = current_time;
    // if calling get my orders, set flag
    else if ( api_command.startsWith( "om" ) )
        is_my_orders_request = true;

    // add to sent queue so we can check if it timed out
    request->time_sent_ms = current_time;

    // remove request tag
    api_command.remove( 0, 3 );

    const bool is_get = api_command.startsWith( "get-" );
    const bool is_post = api_command.startsWith( "post-" );

    if ( is_get )
        api_command.remove( 0, 4 );
    else if ( is_post )
        api_command.remove( 0, 5 );

    const QLatin1String &base_url_str = WAVES_MATCHER_URL;

    // create url which will hold 'url'+'query_args'
    QUrl url = QUrl( base_url_str + api_command );

    // create the request
    QNetworkRequest nam_request;

    // add orders request http headers
    if ( is_my_orders_request )
    {
        const QUrlQuery query = QUrlQuery( "activeOnly=true" );
        url.setQuery( query );

        const QByteArray sign_bytes = account.createGetOrdersBytes( current_time );
        QByteArray signature;

        const bool success = account.sign( sign_bytes, signature );

        if ( !success )
            kDebug() << "local waves error: failed to sign for get my orders request";

        // add signature and timestamp header
        nam_request.setRawHeader( "Signature", QBase58::encode( signature ) );
        nam_request.setRawHeader( "Timestamp", QString::number( current_time ).toLocal8Bit() );
    }
    else
    {
        // add http content json header
        nam_request.setRawHeader( "Content-Type", "application/json;charset=UTF-8" );
    }

    // add http json accept header
    nam_request.setRawHeader( "Accept", "application/json" );

    // set the url
    nam_request.setUrl( url );

    // send REST message
    QNetworkReply *const &reply = is_get  ? nam->get( nam_request ) :
                                  is_post ? nam->post( nam_request, request->body.toLocal8Bit() ) :
                                          nullptr;

    if ( !reply )
    {
        kDebug() << "local error: failed to generate a valid QNetworkReply for api command" << request->api_command;
        return;
    }

    nam_queue_sent.insert( reply, request );
    nam_queue.removeOne( request );
    last_request_sent_ms = current_time;
}

void WavesREST::getOrderStatus( Position * const &pos )
{
    sendRequest( QString( WAVES_COMMAND_GET_ORDER_STATUS )
                  .arg( account.getAliasByAsset( pos->market.getQuote() ) )
                  .arg( account.getAliasByAsset( pos->market.getBase() ) )
                  .arg( pos->order_number ), "", pos );
}

void WavesREST::sendCancel( const QString &order_id, Position * const &pos, const Market &market )
{
    const QByteArray body = account.createCancelBody( order_id.toLocal8Bit() );

    const QString command = QString( WAVES_COMMAND_POST_ORDER_CANCEL )
                             .arg( account.getAliasByAsset( market.getQuote() ) )
                             .arg( account.getAliasByAsset( market.getBase() ) );

    // set is_cancelling
    if ( pos && engine->getPositionMan()->isActive( pos ) )
    {
        pos->is_cancelling = true;

        // set the cancel time once here, and once on send, to avoid double cancel timeouts
        pos->order_cancel_time = QDateTime::currentMSecsSinceEpoch();
    }

    sendRequest( command, body, pos );
}

void WavesREST::sendCancelNonLocal( const QString &order_id, const QString &amount_asset_alias, const QString &price_asset_alias )
{
    const QByteArray body = account.createCancelBody( order_id.toLocal8Bit() );

    const QString command = QString( WAVES_COMMAND_POST_ORDER_CANCEL )
                             .arg( amount_asset_alias )
                             .arg( price_asset_alias );

    kDebug() << "local" << engine->engine_type << "info: sending manual cancel request for order_id" << order_id;
    sendRequest( command, body );
}

void WavesREST::sendBuySell( Position * const &pos, bool quiet )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    const qint64 now = current_time + 60000;
    const qint64 future_29d = current_time + qint64( 60000 ) * 60 * 24 * 29;
    const qint64 future_28d = current_time + qint64( 60000 ) * 60 * 24 * 28;

    MarketInfo &info = engine->getMarketInfo( pos->market );

    // create order body for expiration in 29 days
    const QByteArray body = account.createOrderBody( pos, info.price_ticksize, info.quantity_ticksize, now, future_29d );

    // if the order is already set to expire, keep that time, otherwise set to cancel in 28 days
    if ( pos->max_age_epoch == 0 )
        pos->max_age_epoch = future_28d;

    if ( !quiet )
        kDebug() << QString( "queued          %1" )
                    .arg( pos->stringifyOrderWithoutOrderID() );

    //kDebug() << "sending new order request:" << body;
    sendRequest( WAVES_COMMAND_POST_ORDER_NEW, body, pos );
}

void WavesREST::onNamReply( QNetworkReply * const &reply )
{
    // don't process a reply we aren't tracking
    if ( !nam_queue_sent.contains( reply ) )
        return;

    const QString path = reply->url().path();
    QByteArray data = reply->readAll();

    // parse any possible json in the body
    QJsonDocument body_json = QJsonDocument::fromJson( data );
    QJsonObject result_obj;
    QJsonArray result_arr;

    const bool is_array = body_json.isArray();
    const bool is_object = body_json.isObject();

    // cache result array/object
    if ( is_array )
        result_arr = body_json.array();
    else if ( is_object )
        result_obj = body_json.object();

    Request *const &request = nam_queue_sent.take( reply );
    const QString &api_command = request->api_command;
    const qint64 response_time = QDateTime::currentMSecsSinceEpoch() - request->time_sent_ms;

    avg_response_time.addResponseTime( response_time );

    // print unknown reply
    if ( !is_array && !is_object )
    {
        const bool contains_html = data.contains( QByteArray( "<html" ) ) || data.contains( QByteArray( "<HTML" ) );

        // reduce size of cloudflare errors
        if ( contains_html )
            data = QByteArray( "<html error>" );

        kDebug() << "local warning: nam reply got html reponse for" << path << ":" << data;
    }
    // handle matcher info response
    else if ( api_command.startsWith( "md" ) )
    {
        parseMarketData( result_obj );
    }
    // handle order depth response
    else if ( api_command.startsWith( "ms" ) )
    {
        parseMarketStatus( result_obj, request );
    }
    // handle order status response
    else if ( api_command.startsWith( "os" ) )
    {
        parseOrderStatus( result_obj, request );
    }
    // handle order cancel response
    else if ( api_command.startsWith( "oc" ) )
    {
        parseCancelOrder( result_obj, request );
    }
    // handle order new response
    else if ( api_command.startsWith( "on" ) )
    {
        parseNewOrder( result_obj, request );
    }
    // handle my orders response
    else if ( api_command.startsWith( "om" ) )
    {
        parseMyOrders( result_arr, request->time_sent_ms );
    }
    else
    {
        kDebug() << "local warning: nam reply of unknown command for command:" << api_command << "path:" << path << ":" << data;
    }

    deleteReply( reply, request );
}

void WavesREST::onCheckMarketData()
{
    sendRequest( WAVES_COMMAND_GET_MARKET_DATA );
}

void WavesREST::onCheckTicker()
{
    checkTicker();
}

void WavesREST::checkTicker( bool ignore_flow_control )
{
    if ( !ignore_flow_control && yieldToFlowControl() )
        return;

    // if the next index to query is bad, reset it
    if ( next_ticker_index_to_query >= tracked_markets.size() )
        next_ticker_index_to_query = 0;

    // if tracked markets are empty, skip ticker
    if ( tracked_markets.isEmpty() )
    {
        kDebug() << "local warning: skipping querying ticker because tracked_markets is empty";
        return;
    }

//    kDebug() << "checking next ticker" << tracked_markets.value( next_ticker_index_to_query );

    Market market = tracked_markets.value( next_ticker_index_to_query );

    const QString price_alias = account.getAliasByAsset( market.getBase() );
    const QString amount_alias = account.getAliasByAsset( market.getQuote() );

    const QString ticker_url = QString( WAVES_COMMAND_GET_MARKET_STATUS )
                                .arg( amount_alias )
                                .arg( price_alias );

    sendRequest( ticker_url );

    // iterate index
    next_ticker_index_to_query++;
}

void WavesREST::checkBotOrders( bool ignore_flow_control )
{
    if ( !ignore_flow_control && yieldToFlowControl() )
        return;

    sendRequest( QString( WAVES_COMMAND_GET_MY_ORDERS )
                  .arg( QString( account.publicKeyB58() ) ) );
}

void WavesREST::onCheckBotOrders()
{
    checkBotOrders();
}

void WavesREST::onCheckCancellingOrders()
{
    if ( yieldToFlowControl() )
        return;

    // check for empty cancelling query orders
    if ( cancelling_orders_to_query.size() == 0 )
        return;

    if ( ++last_cancelling_index_checked >= cancelling_orders_to_query.size() )
        last_cancelling_index_checked = 0;

    Position *order_to_check = cancelling_orders_to_query.value( last_cancelling_index_checked );

    // prevent bad access, check if address is still alive
    if ( engine->getPositionMan()->isValid( order_to_check ) )
        getOrderStatus( order_to_check );
    // if it's not valid, remove it from the query list
    else
        cancelling_orders_to_query.removeAt( last_cancelling_index_checked );
}

void WavesREST::parseMarketData( const QJsonObject &info )
{
    //kDebug() << "market data" << info;

    if ( !info.contains( "matcherPublicKey" ) ||
         !info.contains( "markets" ) ||
         !info.value( "markets").isArray() )
    {
        kDebug() << "nam reply error: couldn't find the correct fields in market data";
        return;
    }

    const QString matcher_pubkey = info.value( "matcherPublicKey" ).toString();

    // set matcher public key
    account.setMatcherPublicKeyB58( matcher_pubkey.toLocal8Bit() );

    // regenerate tracked markets each update
    tracked_markets.clear();

    const QJsonArray markets_arr = info.value( "markets" ).toArray();
    for ( QJsonArray::const_iterator i = markets_arr.begin(); i != markets_arr.end(); i++ )
    {
        const QJsonObject market_data = (*i).toObject();

        const QString amount_asset_alias = market_data.value( "amountAsset" ).toString();
        const QString price_asset_alias = market_data.value( "priceAsset" ).toString();
        const Coin price_ticksize = Coin::ticksizeFromDecimals( market_data.value( "priceAssetInfo" ).toObject().value( "decimals" ).toVariant().toULongLong() );
        const Coin amount_ticksize = Coin::ticksizeFromDecimals( market_data.value( "amountAssetInfo" ).toObject().value( "decimals" ).toVariant().toULongLong() );
        const Coin matcher_ticksize = market_data.value( "matchingRules" ).toObject().value( "tickSize" ).toString();

        if ( amount_asset_alias.isEmpty() ||
             price_asset_alias.isEmpty() ||
             price_ticksize.isZeroOrLess() ||
             amount_ticksize.isZeroOrLess() ||
             matcher_ticksize.isZeroOrLess() )
        {
            kDebug() << "nam reply warning: caught empty market data value";
            continue;
        }

        // check if the price and amount currencies are hardcoded in, otherwise skip
        if ( !account.getPriceAssets().contains( price_asset_alias ) ||
             !account.getPriceAssets().contains( amount_asset_alias ) )
            continue;

        const QString price_asset = account.getAssetByAlias( price_asset_alias );
        const QString amount_asset = account.getAssetByAlias( amount_asset_alias );
        Market market = Market( price_asset, amount_asset );

//        kDebug() << market << "matching_ticksize:" << market_data.value( "matchingRules" ).toObject().value( "tickSize" ).toString()
//                 << "price asset:" << price_asset << "price ticksize:" << price_ticksize
//                 << "amount asset:" << amount_asset << "amount ticksize:" << amount_ticksize;

        tracked_markets += market;

        // update market ticksize
        MarketInfo &market_info = engine->getMarketInfo( market );
        market_info.price_ticksize = price_ticksize;
        market_info.quantity_ticksize = amount_ticksize;
        market_info.matcher_ticksize = matcher_ticksize;

        // some of these above values seem like nonsense, but only sometimes?
        // TODO: fix a bug so we don't have to do this
        if ( market == "USDN_BTC" )
            market_info.price_ticksize = CoinAmount::SATOSHI *100;
        else if ( market == "USDN_USDT" )
            market_info.price_ticksize = CoinAmount::SATOSHI;
    }

    // update tickers
    if ( !initial_ticker_update_done )
    {
        for ( int i = 0; i < tracked_markets.size(); i++ )
            checkTicker( true ); // check ticker, ignore flow control = true

        initial_ticker_update_done = true;
    }
}

void WavesREST::parseMarketStatus( const QJsonObject &info, Request *const &request )
{
    /// step 1: extract market out of the url
    QList<QString> url_split = request->api_command.split( QChar( '/' ) );

    if ( url_split.size() != 5 )
    {
        kDebug() << "local waves error: couldn't split market status args";
        return;
    }

    // extract assets
    const QString &amount_asset = url_split.value( 2 );
    const QString &price_asset = url_split.value( 3 );

    Market market = Market( account.getAssetByAlias( price_asset ),
                            account.getAssetByAlias( amount_asset ) );

    // check that market exists
    if ( !engine->getMarketInfoStructure().contains( market ) )
    {
        kDebug() << "local waves warning: caught market" << market << "not in market info structure";
        return;
    }

    /// step 2: read spread values
    if ( !info.contains( "bid" ) ||
         !info.contains( "ask" ) )
    {
        kDebug() << "local waves warning: caught empty bid/ask data";
        return;
    }

    // extract ints from json
    const uint64_t bid_raw = info.value( "bid" ).toVariant().toULongLong();
    const uint64_t ask_raw = info.value( "ask" ).toVariant().toULongLong();

    // apply market ticksize to price (the raw price is a multiple of the ticksize)
    MarketInfo &local_market_info = engine->getMarketInfo( market );
    const Coin bid_price = local_market_info.price_ticksize * bid_raw;
    const Coin ask_price = local_market_info.price_ticksize * ask_raw;

    QMap<QString, TickerInfo> ticker_info;
    ticker_info.insert( market, TickerInfo( bid_price, ask_price ) );

    engine->processTicker( this, ticker_info );
}

/// order book data: now unused
//void WavesREST::parseOrderBookData( const QJsonObject &info )
//{
////    kDebug() << info;

//    if ( !info.value( "bids" ).isArray() ||
//         !info.value( "asks" ).isArray() ||
//         !info.value( "pair" ).isObject() )
//    {
//        kDebug() << "nam reply warning: caught empty bid/ask data";
//        return;
//    }

//    const QJsonArray &bids = info.value( "bids" ).toArray(),
//                     &asks = info.value( "asks" ).toArray();

//    // parse bid/ask price
//    const uint64_t bid_price = bids.first().toObject().value( "price" ).toVariant().toULongLong();
//    const uint64_t ask_price = asks.first().toObject().value( "price" ).toVariant().toULongLong();

//    const QJsonObject &market_info = info.value( "pair" ).toObject();

//    // parse price/amount asset
//    const QString &amount_asset = market_info.value( "amountAsset" ).toString();
//    const QString &price_asset = market_info.value( "priceAsset" ).toString();

//    Market market = Market( account.getAssetByAlias( price_asset ),
//                            account.getAssetByAlias( amount_asset ) );

//    MarketInfo &local_market_info = engine->getMarketInfo( market );
//    const Coin bid_price_coin = local_market_info.price_ticksize * bid_price;
//    const Coin ask_price_coin = local_market_info.price_ticksize * ask_price;

//    QMap<QString, TickerInfo> ticker_info;
//    ticker_info.insert( market, TickerInfo( bid_price_coin, ask_price_coin ) );

//    engine->processTicker( this, ticker_info );
//}

void WavesREST::parseOrderStatus( const QJsonObject &info, Request *const &request )
{
    if ( !info.contains( "status" ) )
    {
        kDebug() << "nam reply warning: caught bad order status data:" << info;
        return;
    }

    // check if we have a position recorded for this request
    if ( !request->pos )
    {
        kDebug() << "local waves error: found response for order status, but postion is null" << info;
        return;
    }

    // check that the position is locally active
    // (this goes off routinely because we only remove the order from querying when getstatus is called)
    if ( !engine->getPositionMan()->isActive( request->pos ) )
    {
        //kDebug() << "local waves warning: found response for order status, but position is not active for order_id" << order_id << info;
        cancelling_orders_to_query.removeOne( request->pos );
        return;
    }

    Position *const &pos = request->pos;
    MarketInfo &market_info = engine->getMarketInfo( pos->market );

    const QString &order_status = info.value( "status" ).toString();
    Coin filled_quantity = market_info.quantity_ticksize * info.value( "filledAmount" ).toVariant().toULongLong();
    //const Coin filled_fee = CoinAmount::SATOSHI * info.value( "filledFee" ).toVariant().toULongLong();

    //kDebug() << "order status" << order_id << ":" << order_status;

    // clamp qty to original amount
    if ( filled_quantity > pos->quantity )
    {
        kDebug() << "local warning: processed filled quantity" << filled_quantity << "greater than position quantity" << pos->quantity << ", clamping to position quantity. ticksize" << market_info.quantity_ticksize << "filled_amount" << info.value( "filledAmount" ).toVariant().toULongLong();
        filled_quantity = pos->quantity;
    }

    // remove it from pending status orders
    if ( pos->is_cancelling )
        cancelling_orders_to_query.removeOne( pos );

    if ( order_status == "Filled" )
    {
        // do single order fill
        engine->processFilledOrders( QVector<Position*>() << pos, FILL_GETORDER );
    }
    // we cancelled the order out but it got filled or cancelled
    else if ( order_status == "Cancelled" )
    {
        // process partially filled amount
        if ( filled_quantity.isGreaterThanZero() )
            engine->updateStatsAndPrintFill( "getorder", pos->market, pos->order_number, pos->side, pos->strategy_tag, Coin(), filled_quantity, pos->price, Coin() );

        engine->processCancelledOrder( pos );
    }
    // if partially filled, wait for complete fill or cancel
}

void WavesREST::parseCancelOrder( const QJsonObject &info, Request *const &request )
{
    const QString &status = info.value( "status" ).toString();

    // if it wasn't cancelled say something
    if ( status != "OrderCanceled" &&
         status != "OrderCancelRejected" )
    {
        kDebug() << "local waves warning: bad cancel reply status:" << status << "info:" << info;
        return;
    }

    Position *const &pos = request->pos;

    // prevent unsafe access
    if ( !pos || !engine->getPositionMan()->isActive( pos ) )
    {
        kDebug() << "successfully cancelled non-local order:" << info;
        return;
    }

    // queue getstatus command
    if ( !cancelling_orders_to_query.contains( pos ) )
        cancelling_orders_to_query += pos;
}

void WavesREST::parseNewOrder( const QJsonObject &info, Request *const &request )
{
    // check if we have a position recorded for this request
    if ( !request->pos )
    {
        kDebug() << "local waves error: found response for queued position, but postion is null" << info;
        return;
    }

    // check that the position is queued and not set
    if ( !engine->getPositionMan()->isQueued( request->pos ) )
    {
        // extract our message
        const QJsonObject &message = info.value( "message" ).toObject();

        // if fields are valid, we receieved a response for a position that no longer exists. cancel it.
        if ( message.contains( "id" ) &&
             message.contains( "amountAsset" ) &&
             message.contains( "priceAsset" ) )
        {
            const QString &order_id = message.value( "id" ).toString();
            const QString &amount_asset_alias = message.value( "amountAsset" ).toString();
            const QString &price_asset_alias = message.value( "priceAsset" ).toString();

            kDebug() << "local waves warning: cancelling new position from response not found in positions_queued" << order_id << amount_asset_alias << price_asset_alias;

            // send cancel request
            sendCancelNonLocal( order_id, amount_asset_alias, price_asset_alias );
        }
        else
            kDebug() << "local waves error: got response for new position without message object" << info;

        return;
    }

    Position *const &pos = request->pos;

    // check for bad or missing fields
    if ( !info.contains( "success" ) ||
         !info.value( "success" ).toBool() ||
         !info.value( "message" ).toObject().contains( "id" ) )
    {
        const QString &message = info.value( "message" ).toString();

        // remove the position if not enough balance (don't clog up queue)
        if ( message.startsWith( "Not enough tradable" ) )
        {
            engine->getPositionMan()->remove( pos );

            const QStringList words = message.split( QChar( ' ' ) );

            // get info on asset that has a short balance
            const QString asset = account.getAssetByAlias( words.value( 10 ) );
            const Coin asset_required = words.value( 9 );
            const Coin asset_available = words.value( 19 );

            // get info on fee
            const QString fee_asset = account.getAssetByAlias( words.value( 13 ) );
            const Coin fee_required = words.value( 13 );
            const Coin fee_available = words.value( 22 );

            // check for valid parse
            if ( asset.isEmpty() || fee_asset.isEmpty() || asset_required.isZeroOrLess() || fee_required.isZeroOrLess() )
            {
                kDebug() << "local waves error: failed to parse low balance message:" << message;
                return;
            }

            if ( fee_available.isZeroOrLess() && fee_required.isGreaterThanZero() )
            {
                kDebug() << "local waves error: not enough fee:" << fee_available << fee_asset;
                return;
            }

            // cancel this asset, but only for flux phases, and only cancel enough to meet asset_required amount, and ban from flux phases for 1hr
            engine->getPositionMan()->cancelFluxOrders( asset, asset_required, 3600 );
            return;
        }

        kDebug() << "local waves error: failed to set new order:" << message;
        return;
    }

    const QString &order_id = info.value( "message" ).toObject().value( "id" ).toString();

    // active pos
    engine->getPositionMan()->activate( pos, order_id );
}

void WavesREST::parseMyOrders( const QJsonArray &orders, qint64 request_time_sent_ms )
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

    for ( QJsonArray::const_iterator i = orders.begin(); i != orders.end(); i++ )
    {
        if ( !(*i).isObject() )
            continue;

        const QJsonObject &order = (*i).toObject();
        const QString &id = order.value( "id" ).toString();
        const QString &side_str = order.value( "type" ).toString().toLower();
        const uint64_t price_raw = order.value( "price" ).toVariant().toULongLong();
        const uint64_t amount_raw = order.value( "amount" ).toVariant().toULongLong();

        const QJsonObject &asset_pair = order.value( "assetPair" ).toObject();

        // parse price/amount asset
        const QString &amount_asset = asset_pair.value( "amountAsset" ).toString();
        const QString &price_asset = asset_pair.value( "priceAsset" ).toString();

        Market market = Market( account.getAssetByAlias( price_asset ),
                                account.getAssetByAlias( amount_asset ) );

        // process price/base amounts
        MarketInfo &local_market_info = engine->getMarketInfo( market );
        const Coin price = local_market_info.price_ticksize * price_raw;
        const Coin base_amount = ( local_market_info.quantity_ticksize * amount_raw ) * price;

        if ( id.isEmpty() || // check for bad id
             asset_pair.isEmpty() || // check empty asset pair
             !market.isValid() || // check for empty assets
             ( side_str != "buy" && side_str != "sell" ) || // check for bad side
             price_raw < 1 || // check for bad raw price
             amount_raw < 1 || // check for bad raw amount
             price.isZeroOrLess() || // check for bad price
             base_amount.isZeroOrLess() ) // check for bad amount
            continue;

        const quint8 side = side_str == "buy" ? SIDE_BUY : SIDE_SELL;

        // insert into seen orders
        order_numbers += id;

        // insert (market, order)
        order_map.insert( market, OrderInfo( id, side, price, base_amount ) );
    }

    engine->processOpenOrders( order_numbers, order_map, request_time_sent_ms );
}

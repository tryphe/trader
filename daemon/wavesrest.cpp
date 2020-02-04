#include "wavesrest.h"
#include "position.h"
#include "positionman.h"
#include "alphatracker.h"
#include "engine.h"
#include "wavesaccount.h"
#include "wavesutil.h"

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
    BaseREST::limit_commands_sent = 15; // stop checks if we are over this many commands sent

    // init asset maps
    account.initAssetMaps();
    // set matcher public key
    account.setMatcherPublicKeyB58( "9cpfKN9suPNvfeUNphzxXMjcnn974eme8ZhWUjaktzU5" );

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
    orderbook_timer->start( WAVES_TIMER_INTERVAL_CHECK_NEXT_ORDER );
    onCheckBotOrders();
#endif

    onCheckMarketData();

    // sendRequest( WAVES_COMMAND_POST_ORDER_NEW, "{\"amount\":9700000,\"assetPair\":{\"amountAsset\":\"WAVES\",\"priceAsset\":\"8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS\"},\"expiration\":1583050061708,\"id\":\"7CrdLzhytNurjrbouN4jXM7xCvVYHTfcYpfgnJuE7QRM\",\"matcherFee\":300000,\"matcherPublicKey\":\"9cpfKN9suPNvfeUNphzxXMjcnn974eme8ZhWUjaktzU5\",\"orderType\":\"sell\",\"price\":1000000,\"proofs\":[\"2ikLEBEGFEW5F5ZfWnNTv8wqvSMhN1x6fZJFQ9FU66cL5EQbi53nidJGHd8kdzutSEtR4PxE7jhjk8mVuWmPMLGs\"],\"senderPublicKey\":\"27YM9icwd6TwfZD3KEJpYsj7rLwPAShJdYXrCt8QRo6L\",\"timestamp\":1580544581708,\"version\":2}" );
}

void WavesREST::sendNamQueue()
{
    // check for requests
    if ( nam_queue.isEmpty() )
        return;

    // stop sending commands if server is unresponsive
    if ( yieldToServer() )
        return;

    // go through requests
    for ( QQueue<Request*>::const_iterator i = nam_queue.begin(); i != nam_queue.end(); i++ )
    {
        Request *const &request = *i;

        // if 2 or more new order commands are in flight, wait for them
        if ( request->api_command.startsWith( "on-" ) && isCommandSent( "on-", 2 ) )
            continue;

        sendNamRequest( request );
        // the request is added to sent_nam_queue and thus not deleted until the response is met
        return;
    }
}

void WavesREST::sendNamRequest( Request * const &request )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    QString api_command = request->api_command;

    // set the order request time for new order
    if ( api_command.startsWith( "on" ) )
        request->pos->order_request_time = current_time;
    // set cancel time properly
    else if ( api_command.startsWith( "oc" ) )
        request->pos->order_cancel_time = current_time;

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

    // inherit the body from the input structure and add it to the url
//    const QUrlQuery query = QUrlQuery( request->body );
//    url.setQuery( query );

//    kDebug() << query.toString();

    // create the request
    QNetworkRequest nam_request;
    nam_request.setUrl( url );

    // add http json accept header
    nam_request.setRawHeader( "Accept", "application/json" );
    nam_request.setRawHeader( "Content-Type", "application/json;charset=UTF-8" );

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

void WavesREST::sendCancel( Position * const &pos )
{
    const QByteArray body = account.createCancelBody( pos->order_number.toLatin1() );

    const QString command = QString( WAVES_COMMAND_POST_ORDER_CANCEL )
                             .arg( account.getAliasByAsset( pos->market.getQuote() ) )
                             .arg( account.getAliasByAsset( pos->market.getBase() ) );

    //kDebug() << "sending cancel request:" << command << body;
    sendRequest( command, body, pos );
}

void WavesREST::sendBuySell( Position * const &pos, bool quiet )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    const qint64 now = current_time + 60000;
    const qint64 future_29d = current_time + ( 60000LL * 60LL * 24LL * 29LL );
    const qint64 future_28d = current_time + ( 60000LL * 60LL * 24LL * 28LL );

    // create order body for expiration in 29 days
    const QByteArray body = account.createOrderBody( pos, now, future_29d );

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
    else if ( api_command.startsWith( "bd" ) )
    {
        parseOrderBookData( result_obj );
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
    else
    {
        kDebug() << "local warning: nam reply of unknown command for" << path << ":" << data;
    }

    deleteReply( reply, request );
}

void WavesREST::onCheckMarketData()
{
    sendRequest( WAVES_COMMAND_GET_MARKET_DATA );
}

void WavesREST::onCheckTicker()
{
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

    const QString ticker_url = QString( WAVES_COMMAND_GET_BOOK_DATA )
                                .arg( amount_alias )
                                .arg( price_alias );

    sendRequest( ticker_url, "depth=1" );

    // iterate index
    next_ticker_index_to_query++;
}

void WavesREST::onCheckBotOrders()
{
    /// step 1: query one active order
    // check for empty positions
    if ( engine->getPositionMan()->active().size() > 0 )
    {
        // TODO: fix this crappy shit
        QList<Position*> pos_list = engine->getPositionMan()->active().toList();

        if ( ++last_index_checked >= pos_list.size() )
            last_index_checked = 0;

        //kDebug() << "checking order" << order_to_check->order_number;

        Position *order_to_check = pos_list.value( last_index_checked );
        getOrderStatus( order_to_check );
    }

    /// step 2: query cancelling orders

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
    const QJsonArray markets_arr = info.value( "markets" ).toArray();

    // regenerated tracked markets each update
    tracked_markets.clear();

    for ( QJsonArray::const_iterator i = markets_arr.begin(); i != markets_arr.end(); i++ )
    {
        const QJsonObject market_data = (*i).toObject();

        const QString amount_asset_alias = market_data.value( "amountAsset" ).toString();
        const QString price_asset_alias = market_data.value( "priceAsset" ).toString();
        const Coin ticksize = market_data.value( "matchingRules" ).toObject().value( "tickSize" ).toString();

        if ( amount_asset_alias.isEmpty() ||
             price_asset_alias.isEmpty() ||
             ticksize.isZeroOrLess() )
        {
            kDebug() << "nam reply warning: caught empty market data value";
            continue;
        }

        // check if the price and amount currencies are hardcoded in, otherwise skip
        if ( !account.getPriceAssets().contains( price_asset_alias ) ||
             !account.getPriceAssets().contains( amount_asset_alias ) )
            continue;

//        kDebug() << "amount_asset_name: " << amount_asset_alias
//                 << "price_asset_name:  " << price_asset_alias;

        Market market = Market( account.getAssetByAlias( price_asset_alias ),
                                account.getAssetByAlias( amount_asset_alias ) );

        tracked_markets += market;

        // update market ticksize
        MarketInfo &market_info = engine->getMarketInfo( market );
        market_info.price_ticksize = ticksize;
    }

    // update tickers
    if ( !initial_ticker_update_done )
    {
        for ( int i = 0; i < tracked_markets.size(); i++ )
            onCheckTicker();

        initial_ticker_update_done = true;
    }
}

void WavesREST::parseOrderBookData( const QJsonObject &info )
{
//    kDebug() << info;

    if ( !info.value( "bids" ).isArray() ||
         !info.value( "asks" ).isArray() ||
         !info.value( "pair" ).isObject() )
    {
        kDebug() << "nam reply warning: caught empty bid/ask data";
        return;
    }

    const QJsonArray &bids = info.value( "bids" ).toArray(),
                     &asks = info.value( "asks" ).toArray();

    // parse bid/ask price
    const Coin bid_price = CoinAmount::SATOSHI * bids.first().toObject().value( "price" ).toInt();
    const Coin ask_price = CoinAmount::SATOSHI * asks.first().toObject().value( "price" ).toInt();

    const QJsonObject &market_info = info.value( "pair" ).toObject();

    // parse price/amount asset
    const QString &amount_asset = market_info.value( "amountAsset" ).toString();
    const QString &price_asset = market_info.value( "priceAsset" ).toString();

    Market market = Market( account.getAssetByAlias( price_asset ),
                            account.getAssetByAlias( amount_asset ) );

    QMap<QString, TickerInfo> ticker_info;
    ticker_info.insert( market, TickerInfo( bid_price, ask_price ) );

    // kDebug() << market << ":" << bid_price << ask_price;

    engine->processTicker( this, ticker_info );
}

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
    const QString &order_status = info.value( "status" ).toString();
    const Coin filled_quantity = CoinAmount::SATOSHI * info.value( "filledAmount" ).toInt();
    const Coin filled_fee = CoinAmount::SATOSHI * info.value( "filledFee" ).toInt();

    //kDebug() << "order status" << order_id << ":" << order_status;

    if ( order_status == "Filled" )
    {
        // do single order fill
        engine->processFilledOrders( QVector<Position*>() << pos, FILL_GETORDER );
    }
    // we cancelled the order out but it got filled or cancelled
    else if ( order_status == "PartiallyFilled" || order_status == "Cancelled" )
    {
        if ( filled_quantity.isGreaterThanZero() )
        {
            //kDebug() << "partially filled order quantity:" << pos->market << filled_quantity;
            engine->updateStatsAndPrintFill( "getorder", pos->market, pos->order_number, pos->side, pos->strategy_tag, Coin(), filled_quantity, pos->price, Coin(), true );
        }

        // if it was cancelled, remove it from pending status orders
        cancelling_orders_to_query.removeOne( pos );

        engine->processCancelledOrder( pos );
    }
}

void WavesREST::parseCancelOrder( const QJsonObject &info, Request *const &request )
{
    const QString &order_id = info.value( "orderId" ).toString();
    const QString &status = info.value( "status" ).toString();

    // if it wasn't cancelled say something
    if ( status != "OrderCanceled" &&
         status != "OrderCancelRejected" )
    {
        kDebug() << "local waves warning: unknown cancel reply status:" << status << ":" << info;
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
        kDebug() << "local waves warning: position from response not found in positions_queued" << info;
        return;
    }

    // check for bad or missing fields
    if ( !info.contains( "success" ) ||
         !info.value( "success" ).toBool() ||
         !info.value( "message" ).toObject().contains( "id" ) )
    {
        kDebug() << "local waves error: failed to set new order:" << info.value( "message" ).toString();
        return;
    }

    Position *const &pos = request->pos;
    const QString &order_id = info.value( "message" ).toObject().value( "id" ).toString();

    // active pos
    engine->getPositionMan()->activate( pos, order_id );
}

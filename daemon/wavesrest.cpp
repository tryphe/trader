#include "wavesrest.h"
#include "position.h"
#include "positionman.h"
#include "alphatracker.h"
#include "engine.h"

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

    exchange_string = Waves_EXCHANGE_STR;
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
    const QString ALIAS_USD =   "Ft8X1v1LTa1ABafufpaCWyVj8KkaxUWE6xBhW6sNFJck",
                  ALIAS_USDN =  "DG2xFkPdDwKUoBkzGAhQtLpSGzfXLiCYPEzeKH2Ad24p",
                  ALIAS_EUR =   "Gtb1WRznfchDnTh37ezoDTJ4wcoKaRsKqKjJjy7nm2zU",
                  ALIAS_CNY =   "DEJbZipbKQjwEiRjx2AqQFucrj5CZ3rAc4ZvFM8nAsoA",
                  ALIAS_TRY =   "2mX5DzVKWrAJw8iwdJnV2qtoeVG9h5nTDpTqC1wb1WEN",
                  ALIAS_BTC =   "8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS",
                  ALIAS_WAVES = "WAVES",
                  ALIAS_ETH =   "474jTeYx2r2Va35794tCScAXWJG9hU2HcgxzMowaZUnu",
                  ALIAS_BCH =   "zMFqXuoyrn5w17PFurTqxB7GsS71fp9dfk6XFwxbPCy",
                  ALIAS_BSV =   "62LyMjcr2DtiyF5yVXFhoQ2q414VPPJXjsNYp72SuDCH",
                  ALIAS_LTC =   "HZk1mbfuJpmxU1Fs4AX5MWLVYtctsNcg6e2C6VKqK8zk",
                  ALIAS_DASH =  "B3uGHFRpSUuGEDWjqB9LWWxafQj8VTvpMucEyoxzws5H",
                  ALIAS_XMR =   "5WvPKSJXzVE2orvbkJ8wsQmmQKqTv9sGBPksV4adViw3",
                  ALIAS_ZEC =   "BrjUWjndUanm5VsJkbUip8VRYy6LWJePtxya3FNv4TQa";

    asset_by_alias.insert( ALIAS_USD, "USD" );
    asset_by_alias.insert( ALIAS_USDN, "USDN" );
    asset_by_alias.insert( ALIAS_EUR, "EUR" );
    asset_by_alias.insert( ALIAS_CNY, "CNY" );
    asset_by_alias.insert( ALIAS_TRY, "TRY" );
    asset_by_alias.insert( ALIAS_BTC, "BTC" );
    asset_by_alias.insert( ALIAS_WAVES, "WAVES" );
    asset_by_alias.insert( ALIAS_ETH, "ETH" );
    asset_by_alias.insert( ALIAS_BCH, "BCH" );
    asset_by_alias.insert( ALIAS_BSV, "BSV" );
    asset_by_alias.insert( ALIAS_LTC, "LTC" );
    asset_by_alias.insert( ALIAS_DASH, "DASH" );
    asset_by_alias.insert( ALIAS_XMR, "XMR" );
    asset_by_alias.insert( ALIAS_ZEC, "ZEC" );

    // add all of the aliases above into price_assets (they are all price assets)
    for ( QMap<QString,QString>::const_iterator i = asset_by_alias.begin(); i != asset_by_alias.end(); i++ )
        price_assets += i.key();

    // duplicate the above map into reverse access map alias_by_asset
    for ( QMap<QString,QString>::const_iterator i = asset_by_alias.begin(); i != asset_by_alias.end(); i++ )
        alias_by_asset.insert( i.value(), i.key() );

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

    onCheckMarketData();
}

void WavesREST::sendNamQueue()
{
    // check for requests
    if ( nam_queue.isEmpty() )
        return;

    // TODO: add rate/alive checking here

    // send the next request
    sendNamRequest( nam_queue.first() );
}

void WavesREST::sendNamRequest( Request * const &request )
{
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    QString api_command = request->api_command;

    const bool is_get = api_command.startsWith( "get-" );

    if ( is_get )
        api_command.remove( 0, 4 );

    // add to sent queue so we can check if it timed out
    request->time_sent_ms = current_time;

    const QLatin1String &base_url_str = WAVES_MATCHER_URL;

    // create url which will hold 'url'+'query_args'
    QUrl url = QUrl( base_url_str + api_command );

    // inherit the body from the input structure and add it to the url
    const QUrlQuery query = QUrlQuery( request->body );
    url.setQuery( query );

    // create the request
    QNetworkRequest nam_request;
    nam_request.setUrl( url );

    // add http json accept header
    nam_request.setRawHeader( "Accept", "application/json" );

    // send REST message
    QNetworkReply *const &reply = is_get? nam->get( nam_request ) :
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
    bool is_json_invalid = false;

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

        kDebug() << "local warning: unknown nam reply for" << path << ":" << data;
    }
    // handle matcher info response
    else if ( api_command == WAVES_COMMAND_GET_MARKET_DATA )
    {
        parseMarketData( result_obj );
    }
    // handle order depth response
    else if ( api_command.startsWith( WAVES_COMMAND_GET_MARKET_DATA ) )
    {
        parseOrderBookData( result_obj );
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

    QString price_alias = alias_by_asset.value( market.getBase() );
    QString amount_alias = alias_by_asset.value( market.getQuote() );

    const QString ticker_url = QString( WAVES_COMMAND_GET_BOOK_DATA )
                                .arg( amount_alias )
                                .arg( price_alias );

    sendRequest( ticker_url, "depth=1" );

    // iterate index
    next_ticker_index_to_query++;
}

void WavesREST::onCheckBotOrders()
{

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
        if ( !price_assets.contains( price_asset_alias ) ||
             !price_assets.contains( amount_asset_alias ) )
            continue;

//        kDebug() << "amount_asset_name: " << amount_asset_alias
//                 << "price_asset_name:  " << price_asset_alias;

        Market market = Market( asset_by_alias.value( price_asset_alias ),
                                asset_by_alias.value( amount_asset_alias ) );

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

    Market market = Market( asset_by_alias.value( price_asset ),
                            asset_by_alias.value( amount_asset ) );

    QMap<QString, TickerInfo> ticker_info;
    ticker_info.insert( market, TickerInfo( bid_price, ask_price ) );

    // kDebug() << market << ":" << bid_price << ask_price;

    engine->processTicker( this, ticker_info );
}

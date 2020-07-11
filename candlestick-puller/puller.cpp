#include "puller.h"

#include "../daemon/coinamount.h"
#include "../daemon/market.h"

#include <QDebug>
#include <QObject>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QFile>
#include <QCoreApplication>

Puller::Puller() :
    QObject()
{
    // initialize nam
    nam = new QNetworkAccessManager();
    connect( nam, &QNetworkAccessManager::finished, this, &Puller::onNamReply );

    // polling timer
    QTimer *ticker_timer = new QTimer( this );
    connect( ticker_timer, &QTimer::timeout, this, &Puller::onCheckFinished );
    ticker_timer->setTimerType( Qt::VeryCoarseTimer );
    ticker_timer->start( 1000 );

    // nam timer
    QTimer *nam_queue_timer = new QTimer( this );
    connect( nam_queue_timer, &QTimer::timeout, this, &Puller::onSendNameQueue );
    nam_queue_timer->setTimerType( Qt::CoarseTimer );
    nam_queue_timer->start( 60 );

    ///
    QString input_market = QCoreApplication::arguments().value( 1 );
    QString input_date = QCoreApplication::arguments().value( 2 );
    QString input_invert_flag = QCoreApplication::arguments().value( 3 );

    // look for continue flag in date argument (if we supply '?', do a date scan without exiting on empty data)
    continue_on_empty_data = input_date.endsWith( "?" );
    if ( continue_on_empty_data )
        input_date.chop( 1 );

    current_market = input_market; // ex: "ZEC-BTC"
    current_date = QDateTime::fromString( input_date, "'M'M'd'd'y'yyyy" ); // ex: "M10d28y2016"
    invert_price = input_invert_flag.toLower() == "invert"; // 0 = don't invert, 1 = invert

    if ( invert_price )
        qDebug() << "info: inverting price data";

    static const QString USAGE_EXAMPLE_STR = "example: ./candlestick-puller ZEC-BTC M10d28y2016";

    // check for valid date
    if ( !current_date.isValid() )
        qFatal( QString( "error: invalid start date: %1" ).arg( USAGE_EXAMPLE_STR ).toLocal8Bit() );

    // check for valid market format
    Market m = Market( current_market );
    if ( !m.isValid() || !current_market.contains( QChar('-') ) )
        qFatal( QString( "error: invalid market: %1 (did you forget the '-'?)" ).arg( USAGE_EXAMPLE_STR ).toLocal8Bit() );

    // construct filename, but reverse the market naming pattern (because it's backwards), except if we are inverting the price
    filename = "BITTREX." + ( invert_price ? m.getBase() : m.getQuote() ) + "_" + ( invert_price ? m.getQuote() : m.getBase() )  + ".5";

    // exit if filename exists
    if ( QFile::exists( filename ) )
        qFatal( QString( "error: file already exists: %1, (re)move it first" ).arg( filename ).toLocal8Bit() );

    sendCandleRequest();
    ///
}

Puller::~Puller()
{
    delete nam;
}

void Puller::sendCandleRequest()
{
    QString addr = QString( "https://api.bittrex.com/v3/markets/%1/candles/MINUTE_5/historical/%2/%3/%4" )
                    .arg( current_market )
                    .arg( current_date.date().year() )
                    .arg( current_date.date().month() )
                    .arg( current_date.date().day() );

    qDebug() << "sending request" << addr;
    sendRequest( addr, "", filename );

    current_date = current_date.addDays( 1 );
    requests_made++;

    if ( current_date >= QDateTime::currentDateTime() )
        waiting_for_final_reply = true;
}

void Puller::sendRequest( QString url, QString query_bytes, QString id )
{
    CandleRequest *req = new CandleRequest();

    req->id = id;
    req->request.setRawHeader( "Content-Type", "application/x-www-form-urlencoded" ); // add content header

    QUrl url_blob( url );
    url_blob.setQuery( QUrlQuery( query_bytes ) );
    req->request.setUrl( url_blob );

    nam_queue += req;
}

void Puller::onSendNameQueue()
{
    if ( nam_queue.isEmpty() )
        return;

    CandleRequest *req = nam_queue.takeFirst();
    QNetworkReply *reply = nam->get( req->request );
    nam_queue_sent.insert( reply, req->id );
    delete req;
}

void Puller::onCheckFinished()
{
    // if not done, return
    if ( !( waiting_for_final_reply && requests_made == requests_parsed ) )
        return;

    // open file
    const QString path = filename;

    QFile savefile( path );
    if ( !savefile.open( QIODevice::WriteOnly | QIODevice::Text ) )
    {
        qDebug() << "error: couldn't open file" << path;
        exit( 0 );
    }

    // construct state
    QString state;
    for ( QVector<Coin>::const_iterator i = samples.begin(); i != samples.end(); i++ )
    {
        if ( !state.isEmpty() )
            state += QChar( ' ' );

        state += (*i).toCompact();
    }
    // prepend a marker, and the start date
    state.prepend( QString( "p %1 " ).arg( start_sample_secs ) );

    // save state
    QTextStream out_samples( &savefile );
    out_samples << state;

    // close file
    out_samples.flush();
    savefile.close();

    // write stuff
    qDebug() << "success! results saved to" << filename;
    exit( 0 );
}

void Puller::onNamReply( QNetworkReply *reply )
{
    // only check for requests that we sent
    if ( !nam_queue_sent.contains( reply ) )
        return;

    QString request_id = nam_queue_sent.take( reply );

    QByteArray raw_data = reply->readAll();
    QJsonDocument json_data = QJsonDocument::fromJson( raw_data );
    bool is_object = json_data.isObject();
    bool is_array = json_data.isArray();

    // cleanup
    reply->deleteLater();

    if ( !is_object && !is_array ) // ensure result is object
    {
        qDebug() << "local error: http response was not jsonable" << raw_data;
        return;
    }

    QJsonObject json_obj;
    QJsonArray json_arr;

    if ( is_object ) json_obj = json_data.object();
    if ( is_array ) json_arr = json_data.array();

    if ( request_id.contains( "BITTREX" ) )
    {
        // throw error on empty first piece, but not if we supplied a continue flag. also process the last day (it's empty)
        if ( json_arr.isEmpty() && !continue_on_empty_data && requests_parsed == 0 )
            qFatal( "error: empty data" );

        QDateTime d;
        for ( QJsonArray::const_iterator i = json_arr.begin(); i != json_arr.end(); i++ )
        {
            QJsonObject candle = (*i).toObject();
            Coin open = candle.value( "open" ).toString();
            Coin high = candle.value( "high" ).toString();
            Coin low = candle.value( "low" ).toString();
            Coin close = candle.value( "close" ).toString();
            d = QDateTime::fromString( candle.value( "startsAt" ).toString(), "yyyy-MM-ddThh:mm:ssZ" );

            if ( open.isZeroOrLess() || close.isZeroOrLess() || high.isZeroOrLess() || low.isZeroOrLess() )
                qFatal( "error: bad sample" );

            // ensure that we don't miss any candles, and they are in order
            if ( last_sample_secs == 0 || d.toSecsSinceEpoch() == last_sample_secs + 300 )
                last_sample_secs = d.toSecsSinceEpoch();
            else
                qFatal( "error: bad time spacing" );

            if ( start_sample_secs == 0 )
                start_sample_secs = d.toSecsSinceEpoch();

            Coin ohlc4 = ( open + high + low + close ) / 4;

            if ( invert_price )
                ohlc4 = CoinAmount::COIN / ohlc4;

            //qDebug() << ohlc4 << d.toSecsSinceEpoch() << "last sample" << last_sample_secs;

            assert( ohlc4.isGreaterThanZero() );
            samples += ohlc4;
        }

        requests_parsed++;

        // print a message depending on if we had data or not
        if ( json_arr.isEmpty() )
            qDebug() << "exchange didn't have any samples for this day, continuing anyway";
        else
            qDebug() << "processed candles for" << d.toString( "dd-MM-yyyy" );

        // if we aren't at the end, send another request
        if ( !waiting_for_final_reply )
            sendCandleRequest();
    }
}

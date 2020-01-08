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
    send_timer->stop();
    orderbook_timer->stop();
    ticker_timer->stop();

    delete send_timer;
    delete orderbook_timer;
    delete ticker_timer;
    send_timer = nullptr;
    orderbook_timer = nullptr;
    ticker_timer = nullptr;

    kDebug() << "[WavesREST] done.";
}

void WavesREST::init()
{
    // we use this to send the requests at a predictable rate
    send_timer = new QTimer( this );
    connect( send_timer, &QTimer::timeout, this, &WavesREST::sendNamQueue );
    send_timer->setTimerType( Qt::CoarseTimer );
    send_timer->start( WAVES_TIMER_INTERVAL_NAM_SEND ); // minimum threshold 200 or so

//    // this timer requests the order book
//    orderbook_timer = new QTimer( this );
//    connect( orderbook_timer, &QTimer::timeout, this, &WavesREST::onCheckBotOrders );
//    orderbook_timer->setTimerType( Qt::VeryCoarseTimer );
//    orderbook_timer->start( BINANCE_TIMER_INTERVAL_ORDERBOOK );

    // this timer reads the lo_sell and hi_buy prices for all coins
    ticker_timer = new QTimer( this );
    connect( ticker_timer, &QTimer::timeout, this, &WavesREST::onCheckTicker );
    ticker_timer->setTimerType( Qt::VeryCoarseTimer );
    ticker_timer->start( WAVES_TIMER_INTERVAL_TICKER );
}

void WavesREST::sendNamQueue()
{

}

void WavesREST::onNamReply( QNetworkReply * const &reply )
{

}

void WavesREST::onCheckBotOrders()
{

}

void WavesREST::onCheckTicker()
{

}

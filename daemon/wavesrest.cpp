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
    kDebug() << "[WavesREST] done.";
}

void WavesREST::init()
{
    // we use this to send the requests at a predictable rate
    connect( send_timer, &QTimer::timeout, this, &WavesREST::sendNamQueue );
    send_timer->start( WAVES_TIMER_INTERVAL_NAM_SEND ); // minimum threshold 200 or so

//    // this timer requests the order book
//    connect( orderbook_timer, &QTimer::timeout, this, &WavesREST::onCheckBotOrders );
//    orderbook_timer->start( BINANCE_TIMER_INTERVAL_ORDERBOOK );

    connect( ticker_timer, &QTimer::timeout, this, &WavesREST::onCheckTicker );
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
#ifndef MISCTYPES_H
#define MISCTYPES_H

#include "position.h"
#include "coinamount.h"

#include <QString>
#include <QByteArray>
#include <QVector>

struct Request
{
    explicit Request()
    {
        time_sent_ms = 0;
        weight = 0;
        pos = nullptr;
    }

    QString api_command;
    QString body;
    qint64 time_sent_ms; // track timeouts
    quint16 weight; // for binance, command weight
    Position *pos;
};

struct MarketInfo
{
    explicit MarketInfo()
    {
        highest_buy = 0.;
        lowest_sell = 0.;

        order_min = 5;
        order_max = 10;
        order_dc = 1;
        order_dc_nice = 0;
        order_landmark_thresh = 0;
        order_landmark_start = 0;
        slippage_timeout = 2 * 60000;
        market_offset = 0.;
        market_sentiment = false;

#if defined(EXCHANGE_BINANCE)
        price_min_mul = 0.2;
        price_max_mul = 5.0;

        price_ticksize = CoinAmount::SATOSHI_STR;
        quantity_ticksize = CoinAmount::SATOSHI_STR;
#endif
    }

    operator QString() const { return QString( "bid %1 ask %2 order_min %3 order_max %4 order_dc %5 order_dc_nice %6 order_landmark_thresh %7 order_landmark_start %8 slippage_timeout %9 market_offset %10 market_sentiment %11" )
                                   .arg( highest_buy, -16 )
                                   .arg( lowest_sell, -16 )
                                   .arg( order_min, -2 )
                                   .arg( order_max, -2 )
                                   .arg( order_dc, -2 )
                                   .arg( order_dc_nice, -2 )
                                   .arg( order_landmark_thresh, -2 )
                                   .arg( order_landmark_start, -2 )
                                   .arg( slippage_timeout, -6 )
                                   .arg( market_offset, -6 )
                                   .arg( market_sentiment ); } // TODO: fill in this string for binance stuff

    // prices for this market
    QVector<QString> order_prices;

    // internal ticker
    Coin highest_buy;
    Coin lowest_sell;

    // ping-pong settings
    QVector<PositionData> /*position_index*/ position_index;
    qint32 /*order count limit*/ order_min;
    qint32 /*order count limit*/ order_max;
    qint32 /*order count combine*/ order_dc;
    qint32 /*nice value*/ order_dc_nice;
    qint32 /*n*/ order_landmark_thresh;
    qint32 /*n*/ order_landmark_start;
    qint32 /*timeout secs*/ slippage_timeout;
    qreal /*offset scalar*/ market_offset;
    bool /*bullish*/ market_sentiment;


#if defined(EXCHANGE_BINANCE)
    // used to pass filter PERCENT_PRICE
    Coin price_min_mul;
    Coin price_max_mul;

    QString price_ticksize; // used to pass filter PRICE_FILTER "tickSize"
    QString quantity_ticksize; // used to pass filter LOT_SIZE "stepSize"
#endif
};

struct OrderInfo
{
    explicit OrderInfo()
    {
    }
    explicit OrderInfo( const QString &_order_number,
                        const quint8 &_side,
                        const QString &_price,
                        const QString &_btc_amount )
    {
        order_number = _order_number;
        side = _side;
        price = _price;
        btc_amount = _btc_amount;
    }

    QString order_number;
    quint8 side;
    QString price;
    QString btc_amount;
};

struct TickerInfo
{
    explicit TickerInfo()
    {
        ask_price = 0.;
        bid_price = 0.;
    }
    explicit TickerInfo( const Coin &_ask_price,
                         const Coin &_bid_price )
    {
        ask_price = _ask_price;
        bid_price = _bid_price;
    }

    operator QString() const { return QString( "bid: %1 ask: %2" )
                                  .arg( bid_price )
                                  .arg( ask_price ); }

    Coin ask_price;
    Coin bid_price;
};

class AvgResponseTime
{
public:
    explicit AvgResponseTime()
        : total( 0 ),
          iterations( 0 )
    {}

    void addResponseTime( quint64 time )
    {
        total += time;
        iterations++;
    }
    quint64 avgResponseTime() const { return iterations == 0 ? 0 : total / iterations; }

private:
    quint64 total, iterations;
};

#endif // MISCTYPES_H

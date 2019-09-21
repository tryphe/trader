#ifndef MISCTYPES_H
#define MISCTYPES_H

#include "coinamount.h"

#include <QString>
#include <QByteArray>

class Position;

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

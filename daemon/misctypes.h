#ifndef MISCTYPES_H
#define MISCTYPES_H

#include "coinamount.h"

#include <QString>
#include <QByteArray>
#include <QVector>
#include <QQueue>

class Position;

struct Request
{
    explicit Request() {}

    QString api_command;
    QString body;
    qint64 time_sent_ms{ 0 }; // track timeouts
    quint16 weight{ 0 }; // for binance, command weight
    Position *pos{ nullptr };
};

struct OrderInfo
{
    explicit OrderInfo( const QString &_order_number,
                        const quint8 &_side,
                        const QString &_price,
                        const QString &_amount )
    {
        order_number = _order_number;
        side = _side;
        price = _price;
        amount = _amount;
    }

    QString order_number;
    quint8 side;
    QString price;
    QString amount;
};

struct Spread
{
    explicit Spread() {}
    explicit Spread( const Coin &_bid_price,
                     const Coin &_ask_price )
    {
        bid = _bid_price;
        ask = _ask_price;
    }

    operator QString() const { return QString( "bid: %1 ask: %2" )
                                  .arg( bid )
                                  .arg( ask ); }

    bool isValid() const { return bid.isGreaterThanZero() && ask.isGreaterThanZero(); }
    Coin getMidPrice() const { return ( ask + bid ) / 2; }

    Coin bid;
    Coin ask;
};

class AvgResponseTime
{
public:
    explicit AvgResponseTime() {}

    void addResponseTime( quint64 time )
    {
        total += time;
        iterations++;
    }
    quint64 avgResponseTime() const { return iterations == 0 ? 0 : total / iterations; }

private:
    quint64 total{ 0 },
            iterations{ 0 };
};

class CoinAverage
{
public:
    explicit CoinAverage() {}

    void addSample( const Coin &sample )
    {
        total += sample;
        iterations++;
    }
    Coin getSignal() const { return iterations == 0 ? CoinAmount::ZERO : total / iterations; }

private:
     Coin total;
     quint64 iterations{ 0 };
};

#endif // MISCTYPES_H

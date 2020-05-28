#ifndef MISCTYPES_H
#define MISCTYPES_H

#include "coinamount.h"

#include <QString>
#include <QByteArray>
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

struct TickerInfo
{
    explicit TickerInfo() {}
    explicit TickerInfo( const Coin &_bid_price,
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
    Coin getAverage() const { return iterations == 0 ? Coin() : total / iterations; }

private:
     Coin total;
     quint64 iterations{ 0 };
};

class CoinMovingAverage
{
public:
    CoinMovingAverage() {}
    CoinMovingAverage( int _samples_max ) { setMaxSamples( _samples_max ); }

    void setMaxSamples( int max ) { samples_max = max; }
    int getMaxSamples() const { return samples_max; }
    int getCurrentSamples() const { return samples.size(); }
    void clear() { samples.clear(); }
    void addSample( const Coin &sample )
    {
        samples.push_back( sample );
        while ( samples_max > 0 && samples.size() > samples_max )
            samples.removeFirst();
    }
    Coin getAverage() const
    {
        if ( samples.size() == 0 )
            return Coin();

        const int samples_size = samples.size();
        Coin total;
        for ( int i = 0; i < samples_size; i++ )
            total += samples.value( i );

        return total / samples_size;
    }

private:
     QQueue<Coin> samples;
     int samples_max{ 0 };
};

struct CoinAverageTester
{
    void test()
    {
        // test basic average
        CoinAverage avg_test;
        avg_test.addSample( Coin("1.5") );
        avg_test.addSample( Coin("2.5") );
        avg_test.addSample( Coin("3.5") );
        avg_test.addSample( Coin("4.5") );
        avg_test.addSample( Coin("5.5") );

        assert( avg_test.getAverage() == Coin("3.5") );

        // test moving average
        CoinMovingAverage mavg_test( 10 );
        mavg_test.addSample( Coin("1") );
        mavg_test.addSample( Coin("2") );
        mavg_test.addSample( Coin("3") );

        assert( mavg_test.getAverage() == Coin("2") );

        mavg_test.addSample( Coin("4") );
        mavg_test.addSample( Coin("5") );
        mavg_test.addSample( Coin("6") );

        assert( mavg_test.getAverage() == Coin("3.5") );

        mavg_test.addSample( Coin("7") );
        mavg_test.addSample( Coin("8") );
        mavg_test.addSample( Coin("9") );
        mavg_test.addSample( Coin("10") );
        mavg_test.addSample( Coin("11") );

        assert( mavg_test.getAverage() == Coin("6.5") ); // note: 2:11 / 10 == 6.5

        mavg_test.addSample( Coin("12") );
        mavg_test.addSample( Coin("13") );

        assert( mavg_test.getAverage() == Coin("8.5") ); // note: 4:13 / 10 == 8.5
        assert( mavg_test.getCurrentSamples() == 10 );
        assert( mavg_test.getMaxSamples() == 10 );

        mavg_test.clear();

        assert( mavg_test.getCurrentSamples() == 0 );
    }
};

#endif // MISCTYPES_H

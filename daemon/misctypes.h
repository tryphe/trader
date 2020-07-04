#ifndef MISCTYPES_H
#define MISCTYPES_H

#include "coinamount.h"

#include <QString>
#include <QByteArray>
#include <QVector>

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
    Coin getSignal() const { return iterations == 0 ? Coin() : total / iterations; }

private:
     Coin total;
     quint64 iterations{ 0 };
};

enum SignalType
{
    SMA,
    WMA,
    EMA,
    RSI
};

#include <QDebug>

struct Signal
{
    static const SignalType RSI_MA_TYPE = SMA;

    explicit Signal( const SignalType _type = SMA, const int _samples_max = 0, const int _general_option_0 = 0 )
        : general_option_0( _general_option_0 )
    {
        setMaxSamples( _samples_max );
        setType( _type );
    }
    ~Signal()
    {
        if ( rsi_ma != nullptr )
            delete rsi_ma;
    }

    void setType( const SignalType _type )
    {
        type = _type;

        // if we supplied a third argument, initialize rsi sma if it's null
        if ( type == RSI && general_option_0 > 0 && rsi_ma == nullptr )
            rsi_ma = new Signal( RSI_MA_TYPE, general_option_0 );
    }
    SignalType getType() const { return type; }

    // these functions make it easier to count iterations between samples (so we know if we should give it a sample or not)
    void setMaxSamples( const int max )
    {
        samples_max = max;

        // to prevent ub, just remove extras here (we might call getSignal before addSample which clears extras otherwise)
        while ( samples_max > 0 && samples.size() > samples_max )
            samples.removeFirst();
    }
    int getMaxSamples() const { return samples_max; }
    int getCurrentSamples() const { return samples.size(); }

    // general options
    void setGeneralOption0( const int val )
    {
        general_option_0 = val;
        if ( rsi_ma != nullptr )
            rsi_ma->setMaxSamples( general_option_0 );
    }
    Coin getRSISMA() const { return ( rsi_ma == nullptr ) ? Coin() : rsi_ma->getSignal(); }
    bool isRSISMAPopulated() const { return rsi_ma != nullptr && rsi_ma->getCurrentSamples() == rsi_ma->getMaxSamples(); }
    bool isRSISMAEnabled() const { return rsi_ma != nullptr; }

    void clear() // note: call clear() before setting new type with setType()
    {
        counter = 0;
        samples.clear();

        if ( type == SMA || type == WMA || type == EMA )
        {
            sum = Coin();
        }
        else if ( type == RSI )
        {
            gain_loss.clear();
            current_avg_gain = Coin();
            current_avg_loss = Coin();

            if ( rsi_ma != nullptr )
                rsi_ma->clear();
        }
    }

    Coin getSignal() const
    {
        const int samples_size = samples.size();

        if ( samples_size < 1 )
            return Coin();

        if ( type == SMA )
        {
            return sum / samples_size;
        }
        else if ( type == RSI && ( current_avg_gain.isGreaterThanZero() || current_avg_loss.isGreaterThanZero() ) )
        {
            // if average loss is 0, avoid div0 and make rs a large number
            const Coin rs = ( current_avg_loss.isZero() ) ? CoinAmount::COIN * 100000 :
                                                            current_avg_gain / current_avg_loss;

            // note: classical RSI code: rsi = CoinAmount::COIN * 100 - ( CoinAmount::COIN * 100 / ( CoinAmount::COIN  + rs ) )
            // we want to return a value from 0-2, >1 oversold, <1 overbought, so we use the formula:
            // rsi == 1 / (2 - (2 / (1 + rs))) == CoinAmount::COIN / ( CoinAmount::COIN * 2 - ( CoinAmount::COIN * 2 / ( CoinAmount::COIN  + rs ) ) )
            // simplified as:
            // rsi == 1 / (2 * rs) + 0.5 == CoinAmount::COIN / ( CoinAmount::COIN * 2 * rs ) + ( CoinAmount::SATOSHI * 50000000 )
            return CoinAmount::COIN / ( CoinAmount::COIN * 2 * rs ) + ( CoinAmount::SATOSHI * 50000000 );
        }
        else if ( type == WMA || type == EMA )
        {
            Coin wt; // weight total
            Coin s; // sum of (sample[n] * weight[n])+...
            Coin w;

            for ( int i = 0; i < samples_size; i++ )
            {
                w = i +1;

                if ( type == EMA )
                    w *= w;

                wt += w;
                s += samples.at( i ) * w;
            }

            return s / wt;
        }

        return Coin();
    }

    void addSample( const Coin &sample )
    {
        samples.push_back( sample );

        if ( type == SMA )
        {
            sum += sample;

            while ( samples_max > 0 && samples.size() > samples_max )
                sum -= samples.takeFirst();

            return;
        }
        else if ( type == RSI )
        {
            const int samples_size = samples.size();

            if ( samples_size > 1 )
            {
                // measure new gain/loss
                const Coin current_gain_loss = samples.at( samples_size -1 ) - samples.at( samples_size -2 );

                // set avg/gain loss for the first time if we don't have one
                if ( current_avg_gain.isZero() )
                {
                    gain_loss += current_gain_loss;

                    // if we have enough gain/loss samples, measure rsi
                    if ( gain_loss.size() == samples_max )
                    {

                        // measure gain/loss
                        Coin total_gain, total_loss;
                        for ( int i = 0; i < gain_loss.size(); i++ )
                        {
                            const Coin &current = gain_loss.at( i );

                            if ( current.isLessThanZero() )
                                total_loss += -current;
                            else
                                total_gain += current;
                        }

                        current_avg_gain = total_gain / samples_max;
                        current_avg_loss = total_loss / samples_max;

                        // we don't use this after this block is executed, clear it
                        gain_loss.clear();
                    }
                }
                else
                {
                    // avg_gain_loss == ( previous * ( period-1 ) + current_gain_loss ) / period
                    current_avg_gain = ( current_avg_gain * ( samples_max -1 ) + ( current_gain_loss.isLessThanZero() ? Coin() : current_gain_loss ) ) / samples_max;
                    current_avg_loss = ( current_avg_loss * ( samples_max -1 ) + ( current_gain_loss.isGreaterThanZero() ? Coin() : -current_gain_loss ) ) / samples_max;

                    // if rsi_ma is not null, add sample
                    if ( rsi_ma != nullptr )
                        rsi_ma->addSample( getSignal() );
                }
            }
        }

        while ( samples_max > 0 && samples.size() > samples_max )
            samples.removeFirst();
    }

    void resetIntervalCounter( const int _counter_max ) { counter = 0; counter_max = _counter_max; }
    void iterateIntervalCounter() { counter++; }
    bool shouldUpdate() const { return counter >= counter_max; }
    bool hasSignal() const { return getSignal().isGreaterThanZero(); }

private:
    // sma/wma/ema
    SignalType type{ SMA };
    int samples_max{ 0 };
    QVector<Coin> samples;

    // sma only
    Coin sum;

    // rsi
    QVector<Coin> gain_loss;
    Coin current_avg_gain, current_avg_loss;

    int counter{ 0 };
    int counter_max{ 0 };

    int general_option_0{ 0 };
    Signal *rsi_ma{ nullptr };
};

struct SignalTest
{
    void test()
    {
        /// test SMA with max samples = 0
        Signal sma( SMA );
        sma.addSample( Coin("1.5") );
        sma.addSample( Coin("2.5") );
        sma.addSample( Coin("3.5") );
        sma.addSample( Coin("4.5") );
        sma.addSample( Coin("5.5") );
        assert( sma.getSignal() == Coin("3.5") );

        /// test SMA with max_samples = 10
        sma = Signal( SMA, 10 );
        sma.addSample( Coin("1") );
        sma.addSample( Coin("2") );
        sma.addSample( Coin("3") );
        assert( sma.getSignal() == Coin("2") );

        sma.addSample( Coin("4") );
        sma.addSample( Coin("5") );
        sma.addSample( Coin("6") );
        assert( sma.getSignal() == Coin("3.5") );

        sma.addSample( Coin("7") );
        sma.addSample( Coin("8") );
        sma.addSample( Coin("9") );
        sma.addSample( Coin("10") );
        sma.addSample( Coin("11") );
        assert( sma.getSignal() == Coin("6.5") ); // note: 2:11 / 10 == 6.5

        sma.addSample( Coin("12") );
        sma.addSample( Coin("13") );
        assert( sma.getSignal() == Coin("8.5") ); // note: 4:13 / 10 == 8.5
        assert( sma.getCurrentSamples() == 10 );
        assert( sma.getMaxSamples() == 10 );

        sma.clear();
        assert( sma.getCurrentSamples() == 0 );

        /// test WMA with samples 2,3
        Signal &wma = sma;
        wma = Signal( WMA, 3 );

        wma.addSample( CoinAmount::COIN *2 );
        wma.addSample( CoinAmount::COIN *3 );
        assert( wma.getSignal() == "2.66666666" ); // note ((2 * 1) + (3 * 2)) / 3 == 2.66

        /// test WMA with samples 2,3,4
        wma.clear();
        wma.addSample( CoinAmount::COIN *2 );
        wma.addSample( CoinAmount::COIN *3 );
        wma.addSample( CoinAmount::COIN *4 );
        assert( wma.getSignal() == "3.33333333" ); // note: ((2 * 1) + (3 * 2) + (4 * 3)) / 6 == 3.33

        /// test WMA with samples 3,4,5
        wma.addSample( CoinAmount::COIN *5 );
        assert( wma.getSignal() == "4.33333333" ); // note: ((3 * 1) + (4 * 2) + (5 * 3)) / 6 == 4.33

        /// test EMA with samples 2,3
        Signal &ema = sma;
        ema = Signal( EMA, 3 );

        ema.addSample( CoinAmount::COIN *2 );
        ema.addSample( CoinAmount::COIN *3 );
        assert( ema.getSignal() == "2.80000000" ); // note: ((2 * 1) + (3 * (2 * 2))) / 5 == 2.8

        /// test EMA with samples 2,3,4
        ema.addSample( CoinAmount::COIN *4 );
        assert( ema.getSignal() == "3.57142857" ); // note: ((2 * 1) + (3 * (2 * 2)) + (4 * (3 * 3))) / 14 == 3.57142857

        /// test RSI
//        Signal &rsi = sma;
//        rsi = Signal( RSI, 14 );

//        rsi.addSample( Coin("44.34" ) );
//        rsi.addSample( Coin("44.09" ) );
//        rsi.addSample( Coin("44.15" ) );
//        rsi.addSample( Coin("43.61" ) );
//        rsi.addSample( Coin("44.33" ) );
//        rsi.addSample( Coin("44.83" ) );
//        rsi.addSample( Coin("45.10" ) );
//        rsi.addSample( Coin("45.42" ) );
//        rsi.addSample( Coin("45.84" ) );
//        rsi.addSample( Coin("46.08" ) );
//        rsi.addSample( Coin("45.89" ) );
//        rsi.addSample( Coin("46.03" ) );
//        rsi.addSample( Coin("45.61" ) );
//        rsi.addSample( Coin("46.28" ) );
//        rsi.addSample( Coin("46.28" ) );
//        assert( rsi.getSignal() == "70.46413502" );
//        rsi.addSample( Coin("46.00" ) );
//        assert( rsi.getSignal() == "66.24961855" );
//        rsi.addSample( Coin("46.03" ) );
//        assert( rsi.getSignal() == "66.48094183" );
//        rsi.addSample( Coin("46.41" ) );
//        assert( rsi.getSignal() == "69.34685316" );
//        rsi.addSample( Coin("46.22" ) );
//        assert( rsi.getSignal() == "66.29471265" );
//        rsi.addSample( Coin("45.64" ) );
//        assert( rsi.getSignal() == "57.91502067" );
    }
};

#endif // MISCTYPES_H

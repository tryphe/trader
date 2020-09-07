#ifndef PRICESIGNAL_H
#define PRICESIGNAL_H

#include "coinamount.h"

#include <functional>

#include <QString>
#include <QVector>
#include <QQueue>
#include <QDebug>

enum PriceSignalType
{
    SMA,      // 0
    WMA,      // 1
    EMA,      // 2
    RSI,      // 3
    SMARatio, // 4
    WMARatio, // 5
    EMARatio, // 6
    RSIRatio  // 7
};

class PriceSignal
{
public:

    PriceSignal( const PriceSignalType _type = SMA, const int _fast_length = 0, const int _slow_length = 0, const Coin &_weight = CoinAmount::COIN );
    ~PriceSignal();

    void setSignalArgs( const PriceSignalType _type, const int _fast_length = 0, const int _slow_length = 0, const Coin &_weight = CoinAmount::COIN );
//    PriceSignalType getType() const { return type; }

    inline void applyWeight( Coin &signal ) const;

    void setMaxSamples( const int max );
    int getMaxSamples() const { return samples_max; }
    int getCurrentSamples() const { return samples.size(); }
    void removeExcessSamples();

    void clear();

    const Coin &getSignal();
    const Coin &getSignalSMA();
    const Coin &getSignalSMAR();
    const Coin &getSignalRSI();
    const Coin &getSignalRSIR();
    const Coin &getSignalRSIRES();
    const Coin &getSignalEMA();
    const Coin &getSignalEMAR();
    const Coin &getSignalWMA();
    const Coin &getSignalWMAR();

    void addSample( const Coin &sample );
    void addSampleSMASMAR( const Coin &sample );
    void addSampleRSIRISR( const Coin &sample );
    void addSampleWMAEMAWMAREMAR( const Coin &sample );

    bool hasSignal() const;

//    void setCounterMax( int max ) { counter_max = std::max( 0, max ); }
//    void iterateCounter() { counter++; }
//    void resetCounter() { counter = 0; }
//    bool shouldUpdateSignal() const {  };

private:
    QVector<std::function<const Coin&(void)>> getsignal_internal;
    QVector<std::function<void(const Coin&)>> addsample_internal;
    Coin getsignal_result;

    PriceSignalType type{ SMA };
    Coin weight{ CoinAmount::COIN };
//    int counter{ 0 }, counter_max{ 0 };

    // sma/wma/ema
    int samples_max{ 0 };
    QQueue<Coin> samples;

    // sma only
    Coin sum;

    // rsi
    QQueue<Coin> gain_loss;
    Coin current_avg_gain, current_avg_loss;

    // ma types
    int i;
    Coin wt, s, w;

    // for ratioized PriceSignals
    int fast_length{ 0 }, slow_length{ 0 };
    PriceSignal *embedded_signal{ nullptr };
};

#endif // PRICESIGNAL_H

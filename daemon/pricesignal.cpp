#include "pricesignal.h"
#include "coinamount.h"

#include <functional>

PriceSignal::PriceSignal(const PriceSignalType _type, const int _fast_length, const int _slow_length, const Coin &_weight)
{
    getsignal_internal += std::bind( &PriceSignal::getSignalSMA, this );
    getsignal_internal += std::bind( &PriceSignal::getSignalWMA, this );
    getsignal_internal += std::bind( &PriceSignal::getSignalEMA, this );
    getsignal_internal += std::bind( &PriceSignal::getSignalRSI, this );
    getsignal_internal += std::bind( &PriceSignal::getSignalSMAR, this );
    getsignal_internal += std::bind( &PriceSignal::getSignalWMAR, this );
    getsignal_internal += std::bind( &PriceSignal::getSignalEMAR, this );
    getsignal_internal += std::bind( &PriceSignal::getSignalRSIR, this );

    using std::placeholders::_1;
    addsample_internal += std::bind( &PriceSignal::addSampleSMASMAR, this, _1 );
    addsample_internal += std::bind( &PriceSignal::addSampleWMAEMAWMAREMAR, this, _1 );
    addsample_internal += std::bind( &PriceSignal::addSampleWMAEMAWMAREMAR, this, _1 );
    addsample_internal += std::bind( &PriceSignal::addSampleRSIRISR, this, _1 );
    addsample_internal += std::bind( &PriceSignal::addSampleSMASMAR, this, _1 );
    addsample_internal += std::bind( &PriceSignal::addSampleWMAEMAWMAREMAR, this, _1 );
    addsample_internal += std::bind( &PriceSignal::addSampleWMAEMAWMAREMAR, this, _1 );
    addsample_internal += std::bind( &PriceSignal::addSampleRSIRISR, this, _1 );

    setSignalArgs( _type, _fast_length, _slow_length, _weight );
}

PriceSignal::~PriceSignal()
{
    if ( embedded_signal != nullptr )
        delete embedded_signal;
}

void PriceSignal::setSignalArgs(const PriceSignalType _type, const int _fast_length, const int _slow_length, const Coin &_weight)
{
    type = _type;
    fast_length = _fast_length;
    slow_length = _slow_length;
    weight = std::max( _weight, CoinAmount::COIN ); // clamp weight minimum to 1

    setMaxSamples( fast_length );
    clear();

    // if ratioized type, initialize embedded slow length
    if ( type > RSI )
    {
        // assert that we initialized the recursive PriceSignal length. slow_length should not be <1, except if RSIRatio, which doesn't require a slow length
        assert( type == RSIRatio || slow_length > 0 );

        // if there's no slow length, don't use embedded PriceSignal
        if ( slow_length < 1 )
            return;

        // if RSIR, embed RSIR, otherwise embed non-ratioized version of the PriceSignal type
        const PriceSignalType embedded_type = type == RSIRatio ? RSIRatio :
                                                            static_cast<PriceSignalType>( type -4 );

        // initialize if null
        if ( embedded_signal == nullptr )
        {
            embedded_signal = new PriceSignal( embedded_type, slow_length );
        }
        // reinitialize if not null
        else
        {
            embedded_signal->setSignalArgs( embedded_type, slow_length );
            embedded_signal->clear();
        }
    }
}

void PriceSignal::applyWeight(Coin &signal) const
{
    if ( signal > CoinAmount::COIN )
        signal *= weight;
    else //if ( ret < CoinAmount::COIN )
        signal /= weight;
}

void PriceSignal::setMaxSamples(const int max)
{
    samples_max = max;

    // to prevent ub, just remove extras here (we might call getSignal before addSample which clears extras otherwise)
    removeExcessSamples();
}

void PriceSignal::removeExcessSamples()
{
    while ( samples_max > 0 && samples.size() > samples_max )
        samples.removeFirst();
}

void PriceSignal::clear() // note: call clear() before setting new type with setType()
{
    samples.clear();

    if ( type < RSI ) // SMA, WMA, EMA
    {
        sum.clear();
    }
    else if ( type == RSI || type == RSIRatio )
    {
        gain_loss.clear();
        current_avg_gain.clear();
        current_avg_loss.clear();

        if ( embedded_signal != nullptr )
            embedded_signal->clear();
    }
    else if ( type > RSI ) // SMARatio, WMARatio, EMARatio
    {
        if ( type == SMARatio )
            sum.clear();

        if ( embedded_signal != nullptr )
            embedded_signal->clear();
    }
}

const Coin &PriceSignal::getSignal()
{
    return getsignal_internal.at( type )();
//    switch ( type )
//    {
//        case SMA:      return getSignalSMA();
//        case SMARatio: return getSignalSMAR();
//        case EMA:      return getSignalEMA();
//        case EMARatio: return getSignalEMAR();
//        case WMA:      return getSignalWMA();
//        case WMARatio: return getSignalWMAR();
//        case RSI:      return getSignalRSI();
//        default: return getSignalRSIR(); // RSIRatio
//    }
}

const Coin &PriceSignal::getSignalSMA()
{
//    qDebug() << &*this << "actual size" << samples.size();

    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    // fast = sum / samples_size;
    getsignal_result = sum / samples_size;
    return getsignal_result;
}

const Coin &PriceSignal::getSignalSMAR()
{
    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    // fast = sum / samples_size;
    w = sum / samples_size;

    // if ratioized PriceSignal, return slow/fast
    s = embedded_signal->getSignal();
    if ( s.isZeroOrLess() || w.isZeroOrLess() )
        return CoinAmount::ZERO;

    // apply weight
    getsignal_result = w / s;
    applyWeight( getsignal_result );
    return getsignal_result;
}

const Coin &PriceSignal::getSignalRSI()
{
    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    if ( !( current_avg_gain.isGreaterThanZero() && current_avg_loss.isGreaterThanZero() ) )
        return CoinAmount::ZERO;

    // rs = gain / loss
    s = current_avg_gain / current_avg_loss;
    // classical RSI: rsi = 100 - (100 / (1 + rs)) == CoinAmount::COIN * 100 - ( CoinAmount::COIN * 100 / ( CoinAmount::COIN  + rs ) )
    static const Coin ONE_HUNDRED = CoinAmount::COIN * 100;
    getsignal_result = ONE_HUNDRED - ( ONE_HUNDRED / ( CoinAmount::COIN  + s ) );
    return getsignal_result;
}

const Coin &PriceSignal::getSignalRSIR()
{
    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    if ( !( current_avg_gain.isGreaterThanZero() && current_avg_loss.isGreaterThanZero() ) )
        return CoinAmount::ZERO;

    // rs = gain / loss
    s = current_avg_gain / current_avg_loss;

    // we want to return a value from 0-2, >1 oversold, <1 overbought, so we use the formula:
    // rsi == 1 / (2 - (2 / (1 + rs))) == CoinAmount::COIN / ( CoinAmount::COIN * 2 - ( CoinAmount::COIN * 2 / ( CoinAmount::COIN  + rs ) ) )
    // simplified as:
    // rsi == 0.5 + 1 / (2 * rs) == ( CoinAmount::SATOSHI * 50000000 ) + CoinAmount::COIN / ( 2 * rs )
    static const Coin HALF_COIN = CoinAmount::SATOSHI * 50000000;
    // fast = HALF_COIN + CoinAmount::COIN / ( s * 2 );
    s += s;
    w = HALF_COIN + CoinAmount::COIN / s;

    if ( slow_length < 1 )
        return w;

    // get embedded PriceSignal and check for zero
    s = embedded_signal->getSignal();
    if ( s.isZeroOrLess() )
        return CoinAmount::ZERO;

    // apply weight
    getsignal_result = w / s;
    applyWeight( getsignal_result );
    return getsignal_result;
}

const Coin &PriceSignal::getSignalEMA()
{
    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    wt.clear();
    s.clear();
    w.clear();

    i = 0;
    const QQueue<Coin>::const_iterator &samples_end = samples.end();
    for ( QQueue<Coin>::const_iterator it = samples.begin(); it != samples_end; ++it )
    {
        w = ++i;
        w *= w;

        wt += w;
        s += *it * w;
    }

    // if we have samples, but the samples are zero, safely return zero
    if ( wt.isZeroOrLess() )
        return CoinAmount::ZERO;

    // fast = s / wt;
    getsignal_result = s / wt;
    return getsignal_result;
}

const Coin &PriceSignal::getSignalEMAR()
{
    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    wt.clear();
    s.clear();
    w.clear();

    i = 0;
    const QQueue<Coin>::const_iterator &samples_end = samples.end();
    for ( QQueue<Coin>::const_iterator it = samples.begin(); it != samples_end; ++it )
    {
        w = ++i;
        w *= w;

        wt += w;
        s += *it * w;
    }

    // if we have samples, but the samples are zero, safely return zero
    if ( wt.isZeroOrLess() )
        return CoinAmount::ZERO;

    // fast = s / wt;
    w = s / wt;

    // if ratioized PriceSignal, return fast/slow
    s = embedded_signal->getSignal();
    if ( s.isZeroOrLess() )
        return CoinAmount::ZERO;

    // ret = fast / slow
    getsignal_result = w / s;
    applyWeight( getsignal_result );
    return getsignal_result;
}

const Coin &PriceSignal::getSignalWMA()
{
    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    wt.clear();
    s.clear();
    w.clear();

    i = 0;
    const QQueue<Coin>::const_iterator &samples_end = samples.end();
    for ( QQueue<Coin>::const_iterator it = samples.begin(); it != samples_end; ++it )
    {
        w += CoinAmount::COIN;

        wt += w;
        s += *it * w;
    }

    // if we have samples, but the samples are zero, safely return zero
    if ( wt.isZeroOrLess() )
        return CoinAmount::ZERO;

    // fast = s / wt;
    getsignal_result = s / wt;
    return getsignal_result;
}

const Coin &PriceSignal::getSignalWMAR()
{
    const int samples_size = samples.size();
    if ( samples_size < 1 )
        return CoinAmount::ZERO;

    wt.clear();
    s.clear();
    w.clear();

    i = 0;
    const QQueue<Coin>::const_iterator &samples_end = samples.end();
    for ( QQueue<Coin>::const_iterator it = samples.begin(); it != samples_end; ++it )
    {
        w += CoinAmount::COIN;

        wt += w;
        s += *it * w;
    }

    // if we have samples, but the samples are zero, safely return zero
    if ( wt.isZeroOrLess() )
        return CoinAmount::ZERO;

    // fast = s / wt;
    w = s / wt;

    // if ratioized PriceSignal, return fast/slow
    s = embedded_signal->getSignal();
    if ( s.isZeroOrLess() )
        return CoinAmount::ZERO;

    // ret = fast / slow
    getsignal_result = w / s;
    applyWeight( getsignal_result );
    return getsignal_result;
}

void PriceSignal::addSample(const Coin &sample)
{
    addsample_internal.at( type )( sample );
}

void PriceSignal::addSampleSMASMAR(const Coin &sample)
{
    samples.push_back( sample );

    // push embedded sample
    if ( embedded_signal != nullptr ) // SMARatio, WMARatio, EMARatio, RSIRatio
        embedded_signal->addSample( sample );

    sum += sample;

    while ( samples_max > 0 && samples.size() > samples_max )
        sum -= samples.takeFirst();
}

void PriceSignal::addSampleRSIRISR(const Coin &sample)
{
    samples.push_back( sample );

    // push embedded sample
    if ( embedded_signal != nullptr ) // SMARatio, WMARatio, EMARatio, RSIRatio
        embedded_signal->addSample( sample );

    const int samples_size = samples.size();

    if ( samples_size < 2 )
        return;

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
            const QQueue<Coin>::const_iterator &end = gain_loss.end();
            for ( QQueue<Coin>::const_iterator it = gain_loss.begin(); it != end; ++it )
            {
                const Coin &current = *it;

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
        current_avg_gain = ( current_avg_gain * ( samples_max -1 ) + ( current_gain_loss.isLessThanZero() ? CoinAmount::ZERO : current_gain_loss ) ) / samples_max;
        current_avg_loss = ( current_avg_loss * ( samples_max -1 ) + ( current_gain_loss.isGreaterThanZero() ? CoinAmount::ZERO : -current_gain_loss ) ) / samples_max;
    }

    removeExcessSamples();
}

void PriceSignal::addSampleWMAEMAWMAREMAR(const Coin &sample)
{
    samples.push_back( sample );

    // push embedded sample
    if ( embedded_signal != nullptr ) // SMARatio, WMARatio, EMARatio, RSIRatio
        embedded_signal->addSample( sample );

    removeExcessSamples();
}

bool PriceSignal::hasSignal() const
{
    bool ret = true;

    // check for empty samples
    if ( samples.isEmpty() )
        ret = false;
    // check for samples < max
    else if ( getCurrentSamples() < samples_max )
        ret = false;
    // check for unpopulated rsi
    else if ( ( type == RSIRatio || type == RSI ) &&
              !( current_avg_gain.isGreaterThanZero() && current_avg_loss.isGreaterThanZero() ) )
        ret = false;
    // incorporate recursive state
    else if ( embedded_signal != nullptr )
        ret &= embedded_signal->hasSignal();

    return ret;
}

#include "sprucev2.h"
#include "coinamount.h"
#include "pricesignal.h"
#include "global.h"
#include "market.h"

#include <functional>

SpruceV2::SpruceV2()
{
//    using std::placeholders::_1;
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc0, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc1, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc2, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc3, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc4, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc5, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc6, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc7, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc8, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc9, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc10, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc11, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc12, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc13, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc14, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc15, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc16, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc17, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc18, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc19, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc20, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc21, this, _1 );
//    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc22, this, _1 );

//    alloc_func = m_allocaton_function_vec.at( m_allocation_function_index );

    /// user settings
    m_order_size_sats_min = m_order_size_sats_max = 500000; // sats
    m_order_nice_buys = "2";
    m_order_nice_sells = "2";
    m_order_nice_zerobound_buys = "0";
    m_order_nice_zerobound_sells = "0";
    m_order_nice_spreadput_buys = "6";
    m_order_nice_spreadput_sells = "6";
    m_order_nice_custom_buys = "100";
    m_order_nice_custom_sells = "100";
    m_order_nice_custom_zerobound_buys = "3";
    m_order_nice_custom_zerobound_sells = "3";
    m_order_greed = "0.95"; // keep our spread at least 1-x% apart
    m_order_greed_minimum = "0.975"; // contract greed up to this value
    m_order_greed_buy_randomness = "0.05";
    m_order_greed_sell_randomness = "0.05"; // randomly subtract tenths of a pct from greed up to this amount

    m_market_buy_max = "0.2";
    m_market_sell_max = "0.2";

    m_phase_alloc_noflux = "0.5";
    m_phase_alloc_flux = "0.5";
}

SpruceV2::~SpruceV2()
{
}

void SpruceV2::clear()
{
    SpruceV2();

    setBaseCurrency( QString() );

    // clear prices
    clearCurrentQtys();
    clearCurrentPrices();

    // clear target qtys
    m_qty_to_sl.clear();

    // clear old stuff
    m_markets_beta.clear();
}

void SpruceV2::buildCache()
{
    m_base_capital_cached = getBaseCapital();
}

void SpruceV2::clearCache()
{
    m_base_capital_cached.clear();
}

void SpruceV2::clearCurrentQtys()
{
    m_current_qty.clear();
}

void SpruceV2::setCurrentQty( const QString &currency, const Coin &qty )
{
//    if ( !m_currencies.contains( currency ) )
//        m_currencies += currency;

    m_current_qty[ currency ] = qty;
}

void SpruceV2::clearCurrentPrices()
{
    m_current_price.clear();
}

bool SpruceV2::calculateAmountToShortLong( bool is_midspread_phase )
{
    // clear qty_to_sl. incase we fail, it should not give any signals.
    m_qty_to_sl.clear();

    // check if there are no averages
    if ( m_average.isEmpty() )
    {
        kDebug() << "local error: tried to calculateAmountToShortLong() but average map is empty";
        return false;
    }

    // check if any of the averages are zero
    for ( QMap<QString, Coin>::const_iterator i = m_average.begin(); i != m_average.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &avg = i.value();

        if ( avg.isZeroOrLess() )
        {
            kDebug() << "local error: m_average for" << currency << "is zero or less";
            return false;
        }
    }

    // check if there are no favorabilities
    if ( m_favorability.isEmpty() )
    {
        kDebug() << "local error: tried to calculateAmountToShortLong() but favorability is empty or alt_alloc <=0";
        return false;
    }

//    kDebug() << "prices:" << m_current_price;

    const QString dollar_currency = "USDN";
    const Coin dollar_avg = m_average[ dollar_currency ];
    const Coin dollar_price = m_current_price[ dollar_currency ];

    if ( dollar_avg.isZeroOrLess() )
    {
        kDebug() << "local error: could not detect average for" << dollar_currency;
        return false;
    }

    if ( dollar_price.isZeroOrLess() )
    {
        kDebug() << "local error: could not detect price for" << dollar_currency;
        return false;
    }

    // recompute btc pair averages in dollar terms
    QMap<QString, Coin> average_dollar_terms;
    for ( QMap<QString, Coin>::const_iterator i = m_average.begin(); i != m_average.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &avg = i.value();

        if ( currency == dollar_currency )
            average_dollar_terms[ dollar_currency ] = CoinAmount::COIN / avg;
        else
            average_dollar_terms[ currency ] = avg / dollar_avg;
    }
//    kDebug() << "avg dollar terms" << average_dollar_terms;

    // recompute prices in dollar terms
    QMap<QString, Coin> price_dollar_terms;
    for ( QMap<QString, Coin>::const_iterator i = m_current_price.begin(); i != m_current_price.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &price = i.value();

        if ( currency == dollar_currency )
            price_dollar_terms[ dollar_currency ] = CoinAmount::COIN / price;
        else
            price_dollar_terms[ currency ] = price / dollar_price;
    }
//    kDebug() << "price dollar terms" << price_dollar_terms;

    // iterate each currency we have, construct a rating
    QMap<QString/*currency*/, Coin> rating;
    Coin lowest_rating;
    for ( QMap<QString/*currency*/, Coin>::const_iterator i = average_dollar_terms.begin(); i != average_dollar_terms.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &dollar_avg = i.value();
        const Coin &dollar_price = price_dollar_terms[ currency ];

        // throw warning message on invalid dollar_avg
        if ( dollar_avg.isZeroOrLess() )
        {
            kDebug() << "local warning: invalid dollar_avg:" << currency << dollar_avg;
            continue;
        }

        // throw warning message on invalid dollar_price
        if ( dollar_price.isZeroOrLess() )
        {
            kDebug() << "local warning: invalid dollar_price:" << currency << dollar_price;
            continue;
        }

        const Coin current_rating = dollar_avg / dollar_price;

        // if dollars, set BTC rating
        if ( currency == dollar_currency )
            rating[ getBaseCurrency() ] = current_rating;
        else
            rating[ currency ] = current_rating;

        // store lowest rating
        if ( lowest_rating.isZero() || current_rating < lowest_rating )
            lowest_rating = current_rating;
    }

//    kDebug() << "ratings:" << rating;

    QMap<QString/*currency*/, Coin> rating_over_lowest;
    for ( QMap<QString/*currency*/, Coin>::const_iterator i = rating.begin(); i != rating.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &rating = i.value();

        const Coin rol = ( rating / lowest_rating ).pow( m_alloc_power );

        rating_over_lowest[ currency ] = rol;
    }

//    kDebug() << "rating_over_lowest:" << rating_over_lowest;

    // compute rol * favorability = RLF, and total RLF
    QMap<QString/*currency*/, Coin> rlf;
    Coin rlft;
    Coin ft;
    for ( QMap<QString/*currency*/, Coin>::const_iterator i = rating_over_lowest.begin(); i != rating_over_lowest.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &rl = i.value();
        const Coin &favorability = m_favorability[ currency ];

        const Coin rlf_current = rl * favorability;

        rlf[ currency ] = rlf_current;

        // record RLFT and FT
        rlft += rlf_current;
        ft += favorability;
    }

//    kDebug() << "rlf:" << rlf;

    // get BTCVT
    const Coin btcvt = getBaseCapital();
    if ( btcvt.isZeroOrLess() )
    {
        kDebug() << "local error: base capital is zero";
        return false;
    }

    QMap<QString/*currency*/, Coin> target_amounts, target_percentages;

    // calculate dollar short ratio, btcvt less dsr, usd alloc
    const Coin &dsr = m_dollar_short_ratio;
    const Coin btcvtldr = btcvt * ( CoinAmount::COIN - dsr );
    const Coin usda = dsr * btcvt;

    target_amounts[ dollar_currency ] = usda;
    target_percentages[ dollar_currency ] = dsr;

//    kDebug() << "btcvt:" << btcvt;

    // compute BTCVT*RLF[currency]/RLFT = base currency alloc
    for ( QMap<QString/*currency*/, Coin>::const_iterator i = rlf.begin(); i != rlf.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &rlf = i.value();

        const Coin target_amount = btcvtldr * rlf / rlft;

        // save to member map
        target_amounts[ currency ] = target_amount;
        target_percentages[ currency ] = target_amount / btcvt;
    }

    // compute target = x / price
    for ( QMap<QString/*currency*/, Coin>::const_iterator i = target_amounts.begin(); i != target_amounts.end(); i++ )
    {
        const QString &currency = i.key();

        if ( currency == getBaseCurrency() )
            continue;

        const Coin &amount = i.value();
        const Coin target_qty = amount / m_current_price[ currency ];

        m_qty_to_sl[ Market( m_base_currency, currency ) ] = getCurrentQty( currency ) - target_qty;
    }

    // if noflux phase, record end status
    if ( is_midspread_phase )
    {
        m_target_amounts = target_amounts;
        m_target_percentages = target_percentages;

//        kDebug() << "target amounts:" << m_target_amounts;
//        kDebug() << "target percentages:" << m_target_percentages;
//        kDebug() << "m_qty_to_sl:" << m_qty_to_sl;
    }

    return true;
}

void SpruceV2::adjustCurrentQty( const QString &currency, const Coin &qty )
{
    m_current_qty[ currency ] += qty;
}

//void SpruceV2::setAllocationFunction( const int index )
//{
//    m_allocation_function_index = std::min( std::max( 0, index ), m_allocaton_function_vec.size() -1 );
//    alloc_func = m_allocaton_function_vec.at( m_allocation_function_index );
//}

//QString SpruceV2::getVisualization()
//{
//    QString ret;

//    return ret;
//}

Coin SpruceV2::getBaseCapital() const
{
    // return cached value if we built the cache for this phase
    if ( m_base_capital_cached.isGreaterThanZero() )
        return m_base_capital_cached;

    Coin ret;
    const QMap<QString, Coin>::const_iterator end = m_current_qty.end();
    for( QMap<QString, Coin>::const_iterator i = m_current_qty.begin(); i != end; i++ )
    {
        const QString &currency = i.key();

        // base has price of 1, just add it
        if ( currency == m_base_currency )
        {
            ret += i.value();
            continue;
        }

        const Coin &price = m_current_price.value( currency );

        // check for valid price
        if ( price.isZeroOrLess() )
            return CoinAmount::ZERO;

        ret += i.value() * price;
    }

    return ret;
}

Coin SpruceV2::getExchangeAllocation( const QString &exchange_market_key, bool is_noflux_phase )
{
    return per_exchange_market_allocations.value( exchange_market_key ) * ( is_noflux_phase ? m_phase_alloc_noflux : m_phase_alloc_flux );
}

void SpruceV2::setExchangeAllocation( const QString &exchange_market_key, const Coin allocation )
{
    per_exchange_market_allocations.insert( exchange_market_key, allocation );

    kDebug() << "[SpruceV2] exchange allocation for" << exchange_market_key << ":" << allocation;
}

void SpruceV2::setPhaseAllocation( const Coin &noflux_alloc, const Coin &flux_alloc )
{
    m_phase_alloc_noflux = noflux_alloc;
    m_phase_alloc_flux = flux_alloc;

    if ( noflux_alloc + flux_alloc != CoinAmount::COIN )
        kDebug() << "local warning: total phase allocations != 1";
}

Coin SpruceV2::getOrderGreedRandom( quint8 side ) const
{
    // if greed is unset, just return m_order_greed
    if ( !m_order_greed.isGreaterThanZero() )
        return m_order_greed;

    // try to generate rand range
    const Coin iter = CoinAmount::COIN / 1000;
    const Coin &randomness = side == SIDE_BUY ? m_order_greed_buy_randomness : m_order_greed_sell_randomness;
    const quint32 range = ( randomness / iter ).toUInt32();

    // if the range is 0, return here to prevent a negative spread
    if ( range == 0 )
        return m_order_greed;

    const quint32 rand = Global::getSecureRandomRange32( 0, range );
    const Coin ret = m_order_greed - ( iter * rand );

    // don't return negative or zero greed value
    return ret.isZeroOrLess() ? m_order_greed : ret;
}

void SpruceV2::setOrderNice( const quint8 side, Coin nice, bool midspread_phase )
{
    ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_buys = nice : m_order_nice_custom_sells = nice :
                          ( side == SIDE_BUY ) ? m_order_nice_buys = nice : m_order_nice_sells = nice;
}

Coin SpruceV2::getOrderNice( const QString &currency, const quint8 side, bool midspread_phase )
{
    // get base nice value for side
    const Coin base = ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_buys : m_order_nice_custom_sells :
                                            ( side == SIDE_BUY ) ? m_order_nice_buys : m_order_nice_sells;

    // apply per-market offset to side base
    const Coin base_with_offset = ( side == SIDE_BUY ) ? base + m_order_nice_market_offset_buys.value( Market( m_base_currency, currency ) ) :
                                                         base + m_order_nice_market_offset_sells.value( Market( m_base_currency, currency ) );

    // apply snapback ratio (or not) to base+offset
    Coin ret = ( getSnapbackState( Market( m_base_currency, currency ), side ) ) ? base_with_offset * m_snapback_ratio :
                                                                                   base_with_offset;

    // scale the value based on how much allocation the market gets
    ret *= getTargetPercentage( currency );

    // scale the value to base capital
    ret *= getBaseCapital();

    return ret;
}

void SpruceV2::setOrderNiceZeroBound( const quint8 side, Coin nice, bool midspread_phase )
{
    ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys = nice : m_order_nice_custom_zerobound_sells = nice :
                          ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys = nice : m_order_nice_zerobound_sells = nice;
}

Coin SpruceV2::getOrderNiceZeroBound( const QString &currency, const quint8 side, bool midspread_phase ) const
{
    Coin ret = ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys : m_order_nice_custom_zerobound_sells :
                                           ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys : m_order_nice_zerobound_sells;

    ( side == SIDE_BUY ) ? ret += m_order_nice_market_offset_zerobound_buys.value( Market( m_base_currency, currency ) ) :
                           ret += m_order_nice_market_offset_zerobound_sells.value( Market( m_base_currency, currency ) );

    ret *= getTargetPercentage( currency );

    return ret;
}

void SpruceV2::setOrderNiceSpreadPut( const quint8 side, Coin nice )
{
    ( side == SIDE_BUY ) ? m_order_nice_spreadput_buys = nice :
                           m_order_nice_spreadput_sells = nice;
}

Coin SpruceV2::getOrderNiceSpreadPut( const quint8 side ) const
{
    return ( side == SIDE_BUY ) ? m_order_nice_spreadput_buys * /*getTargetPercentage( currency ) * */getBaseCapital() :
                                  m_order_nice_spreadput_sells * /*getTargetPercentage( currency ) * */ getBaseCapital();
}

void SpruceV2::setSnapbackState( const QString &market, const quint8 side, const bool state, const Coin &price, const Coin &amount_to_shortlong_abs )
{
    // store last trigger2 failure message, but clear when the entire mechanism resets below
    static QMap<QString, qint64> last_trigger2_message;

    // if we are turning the state on, and the state of the other side of this market is enabled, disable it directly
    // note: this should almost never happen, as once the amount_to_sl is under the nice limit, snapback  will disable.
    //       if it flips to the other side before disabling, it won't catch the check and we must disable it here.
    const bool opposite_side = ( side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY;
    const bool other_side_state = getSnapbackState( market, opposite_side );
    if ( state && other_side_state )
    {
        opposite_side == SIDE_BUY ? m_snapback_state_buys[ market ] = false :
                                    m_snapback_state_sells[ market ] = false;

        const QString description_str_disable = QString( "[%1 %2 @ %3]" )
                                                 .arg( market )
                                                 .arg( ( opposite_side == SIDE_BUY ) ? "buys" : "sells" )
                                                 .arg( price );

        kDebug() << QString( "[Diffusion] %1 Snapback disabled for opposite side" ).arg( description_str_disable );
    }

    const qint64 current_time = QDateTime::currentSecsSinceEpoch();

    const QString description_str = QString( "[%1 %2 @ %3]" )
                                     .arg( market )
                                     .arg( ( side == SIDE_BUY ) ? "buys" : "sells" )
                                     .arg( price );

    // prevent bad ref
    if ( side == SIDE_BUY && !m_snapback_trigger1_timequotient_buys.contains( market ) )
    {
        m_snapback_trigger1_timequotient_buys.insert( market, 0 );
        m_snapback_trigger1_count_buys.insert( market, 0 );
    }
    else if ( side == SIDE_SELL && !m_snapback_trigger1_timequotient_sells.contains( market ) )
    {
        m_snapback_trigger1_timequotient_sells.insert( market, 0 );
        m_snapback_trigger1_count_sells.insert( market, 0 );
    }

    // cache trigger1 refs so we can easily check/modify them
    qint64 &trigger1_last_time_quotient = ( side == SIDE_BUY ) ? m_snapback_trigger1_timequotient_buys[ market ] :
                                                                 m_snapback_trigger1_timequotient_sells[ market ];
    qint64 &trigger1_count = ( side == SIDE_BUY ) ? m_snapback_trigger1_count_buys[ market ] :
                                                    m_snapback_trigger1_count_sells[ market ];

    // the key for incrementing to the next step is quotient = current secs / 600
    const qint64 current_time_quotient = current_time / m_snapback_trigger1_time_window_secs;

    // cache trigger2 count, the number of amount_to_sl_abs samples
    const int trigger2_count = ( side == SIDE_BUY ) ? m_snapback_trigger2_sl_abs_ma_buys.value( market ).getCurrentSamples() :
                                                      m_snapback_trigger2_sl_abs_ma_sells.value( market ).getCurrentSamples();

    // reset mechanisms if we are disabling, OR have a new time quotient, except if mechanism #2 triggered
    if ( !state || ( state && trigger1_last_time_quotient < current_time_quotient && trigger2_count == 0 ) )
    {
        // reset trigger #1, also update time quotient when enabling state
        trigger1_count = 0;
        if ( state )
            trigger1_last_time_quotient = current_time_quotient;

        // reset trigger #2 ma
        side == SIDE_BUY ? m_snapback_trigger2_sl_abs_ma_buys[ market ] =  PriceSignal( HMA, m_snapback_trigger2_ma_samples ) :
                           m_snapback_trigger2_sl_abs_ma_sells[ market ] =  PriceSignal( HMA, m_snapback_trigger2_ma_samples );

        // reset trigger #2 failure message
        last_trigger2_message[ market ] = 0;

        kDebug() << QString( "[Diffusion] %1 Snapback triggers reset for %2" )
                     .arg( description_str )
                     .arg( market );
    }

    // do trigger
    if ( state )
    {
        PriceSignal &amount_to_sl_abs_signal = ( side == SIDE_BUY ) ? m_snapback_trigger2_sl_abs_ma_buys[ market ] :
                                                                      m_snapback_trigger2_sl_abs_ma_sells[ market ];

        // iterate trigger mechanism #1 and return if we didn't hit the threshold
        if ( trigger1_count < m_snapback_trigger1_iterations )
        {
            trigger1_count++;
            kDebug() << QString( "[Diffusion] %1 Snapback trigger #1 iteration %2" )
                         .arg( description_str )
                         .arg( trigger1_count );

            // set the trigger #2 trigger price, because next round it will be enabled
            if ( trigger1_count == m_snapback_trigger1_iterations )
                side == SIDE_BUY ? m_snapback_trigger2_trigger_sl_abs_initial_buys[ market ] = amount_to_shortlong_abs :
                                   m_snapback_trigger2_trigger_sl_abs_initial_sells[ market ] = amount_to_shortlong_abs;

            return;
        }
        // after 10 iterations in a 10 minute period for mechanism #1, wait for amount_to_sl pullback trigger mechanism #2
        else
        {
            // if we changed the sample setting since we initialized, set the count manually
            if ( amount_to_sl_abs_signal.getMaxSamples() != m_snapback_trigger2_ma_samples )
                 amount_to_sl_abs_signal.setMaxSamples( m_snapback_trigger2_ma_samples );

            // add sample
            amount_to_sl_abs_signal.addSampleHMA( amount_to_shortlong_abs );

            // trigger2 is finished if:
            // amount to shortlong abs < average of the last SNAPBACK_TRIGGER2_MA_SAMPLES * SNAPBACK_TRIGGER2_MA_RATIO
            // OR
            // we crossed the original trigger #2 price * SNAPBACK_TRIGGER2_PRICE_RATIO;
            QString trigger2_description_str;
            bool trigger2 = false;
            const Coin threshold1 = m_snapback_trigger2_ma_ratio * amount_to_sl_abs_signal.getSignalHMA();
            const Coin threshold2 = m_snapback_trigger2_initial_ratio * ( side == SIDE_BUY ?
                                    m_snapback_trigger2_trigger_sl_abs_initial_buys.value( market ) :
                                    m_snapback_trigger2_trigger_sl_abs_initial_sells.value( market ) );

            if ( amount_to_shortlong_abs < threshold1 ||
                 amount_to_shortlong_abs < threshold2  )
            {
                trigger2 = true;
                trigger2_description_str = "crossed either threshold";

                // clear samples so the next time we trigger mechanism #1, #2 trigger2_count check above is zero and we reset both mechanisms
                amount_to_sl_abs_signal.clear();
            }
            else
            {
                trigger2_description_str = "is still above thresholds";
            }

            // measure the last time we printed a trigger2 failure message, and show the message if it's too old
            bool show_trigger2_message = false;
            if ( !trigger2 && last_trigger2_message.value( market, 0 ) < current_time - m_snapback_trigger2_message_interval )
            {
                last_trigger2_message[ market ] = current_time;
                show_trigger2_message = true;
            }

            // don't show the message every time unless trigger2 is true, otherwise show every 5 minutes
            if ( trigger2 || show_trigger2_message )
                kDebug() << QString( "[Diffusion] %1 Snapback trigger #2 %2 %3 / %4 with sl abs %5" )
                            .arg( description_str )
                            .arg( trigger2_description_str )
                            .arg( Coin( threshold1 ).toString( 4 ) )
                            .arg( Coin( threshold2 ).toString( 4 ) )
                            .arg( Coin( amount_to_shortlong_abs ).toString( 4 ) );

            // if we didn't trigger the mechanism, return early
            if ( !trigger2 )
                return;
        }
    }

    // set state
    side == SIDE_BUY ? m_snapback_state_buys[ market ] = state :
                       m_snapback_state_sells[ market ] = state;

    kDebug() << QString( "[Diffusion] %1 Snapback %2" )
                 .arg( description_str )
                 .arg( ( state ) ? "enabled" : "disabled" );
}

bool SpruceV2::getSnapbackState( const QString &market, const quint8 side ) const
{
    return ( side == SIDE_BUY ) ? m_snapback_state_buys.value( market, false ) :
                                  m_snapback_state_sells.value( market, false );
}

void SpruceV2::addMarketBeta( Market m )
{
    Q_UNUSED( m )

//    if ( !original_quantity.contains( m.getBase() ) ||
//         !original_quantity.contains( m.getQuote() ) )
//    {
//        kDebug() << "SpruceV2 error: couldn't find currency" << m.getBase() << "or" << m.getQuote() << "in" << original_quantity;
//        return;
//    }

//    if ( !m_markets_beta.contains( m ) )
//        m_markets_beta += m;
}

QVector<QString> SpruceV2::getMarketsAlpha() const
{
    QVector<QString> ret;
    const QVector<QString> &keys = m_current_qty.keys().toVector();
    for ( QVector<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
        if ( *i != m_base_currency )
            ret += Market( m_base_currency, *i );

    return ret;
}

bool SpruceV2::isActive()
{
    return !( m_current_qty.isEmpty() || getBaseCurrency() == "disabled" );
}

QString SpruceV2::getSaveState()
{
    QString ret;

    // save interval
    ret += QString( "setspruceinterval %1\n" ).arg( m_interval_secs );

    // save base
    ret += QString( "setsprucebasecurrency %1\n" ).arg( getBaseCurrency() );

    // save spread tolerances
    ret += QString( "setspruceordergreed %1 %2 %3 %4\n" )
            .arg( m_order_greed )
            .arg( m_order_greed_minimum )
            .arg( m_order_greed_buy_randomness )
            .arg( m_order_greed_sell_randomness );

    // save order size
    ret += QString( "setspruceordersize %1 %2\n" ).arg( CoinAmount::SATOSHI * m_order_size_sats_min )
                                                  .arg( CoinAmount::SATOSHI * m_order_size_sats_max );

    // save order count
    ret += QString( "setspruceordercount %1 %2\n" ).arg( m_orders_per_side_flux )
                                                   .arg( m_orders_per_side_midspread );

    // save order timeouts
    ret += QString( "setspruceordertimeout %1 %2 %3\n" ).arg( m_order_timeout_flux_min )
                                                        .arg( m_order_timeout_flux_max )
                                                        .arg( m_order_timeout_midspread );

    // save order size for normal phases
    ret += QString( "setspruceordernice %1 %2 %3 %4 %5 %6\n" ).arg( m_order_nice_buys )
                                                    .arg( m_order_nice_zerobound_buys )
                                                    .arg( m_order_nice_spreadput_buys )
                                                    .arg( m_order_nice_sells )
                                                    .arg( m_order_nice_zerobound_sells )
                                                    .arg( m_order_nice_spreadput_sells );

    // save order size for custom phase
    ret += QString( "setspruceordernicecustom %1 %2 %3 %4\n" ).arg( m_order_nice_custom_buys )
                                                    .arg( m_order_nice_custom_zerobound_buys )
                                                    .arg( m_order_nice_custom_sells )
                                                    .arg( m_order_nice_custom_zerobound_sells );

    // save snapback ratio
    ret += QString( "setsprucesnapback %1\n" ).arg( m_snapback_ratio );

    // save snapback trigger 1 settings
    ret += QString( "setsprucesnapbacktrigger1 %1 %2\n" ).arg( m_snapback_trigger1_time_window_secs )
                                                         .arg( m_snapback_trigger1_iterations );

    // save snapback trigger 2 settings
    ret += QString( "setsprucesnapbacktrigger2 %1 %2 %3 %4\n" ).arg( m_snapback_trigger2_ma_samples )
                                                               .arg( m_snapback_trigger2_ma_ratio )
                                                               .arg( m_snapback_trigger2_initial_ratio )
                                                               .arg( m_snapback_trigger2_message_interval );

    // save phase allocations
    ret += QString( "setsprucephaseallocation %1 %2\n" ).arg( m_phase_alloc_noflux )
                                                        .arg( m_phase_alloc_flux );

    // save scoring power
    ret += QString( "setspruceallocpower %1\n" ).arg( m_alloc_power );

    // save manual dollar ratio
    ret += QString( "setsprucedollarratio %1\n" ).arg( m_dollar_short_ratio );

    // save currency favorability map
    for ( QMap<QString,Coin>::const_iterator i = m_favorability.begin(); i != m_favorability.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &favorability = i.value();

        ret += QString( "setsprucefavorability %1 %2\n" ).arg( currency )
                                                         .arg( favorability );
    }

    // save currency longterm signal map
    for ( QMap<QString,Coin>::const_iterator i = m_average.begin(); i != m_average.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &signal = i.value();

        ret += QString( "setsprucelongtermsignal %1 %2\n" ).arg( currency )
                                                           .arg( signal );
    }

    // save order nice market offsets
    for ( QMap<QString,Coin>::const_iterator i = m_order_nice_market_offset_buys.begin(); i != m_order_nice_market_offset_buys.end(); i++ )
    {
        const QString &market = i.key();
        const Coin &offset_buys = i.value();
        const Coin &offset_sells = m_order_nice_market_offset_sells.value( market );
        const Coin &offset_zerobound_buys = m_order_nice_market_offset_zerobound_buys.value( market );
        const Coin &offset_zerobound_sells = m_order_nice_market_offset_zerobound_sells.value( market );

        // skip if all values are 0
        if ( offset_buys.isZero() &&
             offset_sells.isZero() &&
             offset_zerobound_buys.isZero() &&
             offset_zerobound_sells.isZero() )
            continue;

        ret += QString( "setspruceordernicemarketoffset %1 %2 %3 %4 %5\n" ).arg( market )
                                                                           .arg( offset_buys )
                                                                           .arg( offset_zerobound_buys )
                                                                           .arg( offset_sells )
                                                                           .arg( offset_zerobound_sells );
    }

    // save per-exchange allocations
    for ( QMap<QString,Coin>::const_iterator i = per_exchange_market_allocations.begin(); i != per_exchange_market_allocations.end(); i++ )
    {
        ret += QString( "setspruceallocation %1 %2\n" )
                .arg( i.key() )
                .arg( i.value() );
    }

    // save current quantities
    for ( QMap<QString, Coin>::const_iterator i = m_current_qty.begin(); i != m_current_qty.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &qty = i.value();

        ret += QString( "setspruceqty %1 %2\n" )
                .arg( currency )
                .arg( qty );
    }

    // save beta markets
    for ( QList<Market>::const_iterator i = m_markets_beta.begin(); i != m_markets_beta.end(); i++ )
    {
        ret += QString( "setsprucebetamarket %1\n" )
                .arg( *i );
    }

    return ret;
}

void SpruceV2::setOrderSize( Coin min, Coin max )
{
    // if max is 0, set max to min
    if ( max.isZeroOrLess() )
        max = min;

    // set min and max but clamp values to 64 bits because they are range inputs into our rand functions
    m_order_size_sats_min = std::min( std::numeric_limits<quint64>::max(), min.toUIntSatoshis() );
    m_order_size_sats_max = std::min( std::numeric_limits<quint64>::max(), max.toUIntSatoshis() );
}

Coin SpruceV2::getOrderSize() const
{
    return m_order_size_sats_min == m_order_size_sats_max ? CoinAmount::SATOSHI * m_order_size_sats_min :
           CoinAmount::SATOSHI * Global::getSecureRandomRange64( m_order_size_sats_min, m_order_size_sats_max );
}

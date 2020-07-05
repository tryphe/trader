#include "sprucev2.h"
#include "coinamount.h"
#include "global.h"
#include "market.h"

#include <functional>

SpruceV2::SpruceV2()
{
    using std::placeholders::_1;
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc0, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc1, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc2, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc3, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc4, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc5, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc6, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc7, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc8, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc9, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc10, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc11, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc12, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc13, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc14, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc15, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc16, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc17, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc18, this, _1 );
    m_allocaton_function_vec += std::bind( &SpruceV2::allocationFunc19, this, _1 );

    /// user settings
    m_order_size = "0.005";
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
    clearSignalPrices();

    // clear target qtys
    m_qty_to_sl.clear();

    // clear old stuff
    m_markets_beta.clear();
}

void SpruceV2::clearCurrentQtys()
{
    m_current_qty.clear();
}

void SpruceV2::clearPrices()
{
    m_current_price.clear();
    m_signal_price.clear();
}

void SpruceV2::clearCurrentPrices()
{
    m_current_price.clear();
}

void SpruceV2::clearSignalPrices()
{
    m_signal_price.clear();
}

bool SpruceV2::calculateAmountToShortLong()
{
    const QList<QString> &clist = m_current_qty.keys();
    Coin base_capital; // cache base capital (q*p)
    int base_currency_index;

    // compute row of ratios for base currency
    QVector<Coin> base_row;
    for ( QList<QString>::const_iterator i = clist.begin(); i != clist.end(); i++ )
    {
        const QString &currency = *i;
        const Coin &qty = m_current_qty.value( currency );

        if ( currency == m_base_currency )
        {
            base_currency_index = base_row.size();
            base_row += CoinAmount::COIN;
            base_capital += qty;
            continue;
        }

        const Coin &price = m_current_price.value( currency );
        const Coin &signal_price = m_signal_price.value( currency );

        if ( price.isZeroOrLess() || signal_price.isZeroOrLess() )
        {
            kDebug() << "[Diffusion] error: invalid price for" << currency;
            return false;
        }

        base_row += signal_price;
        base_capital += price * qty;
    }

    //kDebug() << "base row:" << base_row;

    // compute row products, and generate a cross-table for looking at externally (we don't need it, but it's nice to look at)
    QVector<Coin> rp;
    int i, j, rp_idx;
    for ( i = 0; i < base_row.size(); i++ )
    {
        // compute cross-ratio and row product
        rp += CoinAmount::COIN;
        rp_idx = rp.size() -1;
        Coin &row_product = rp[ rp_idx ];

        for ( j = 0; j < base_row.size(); j++ )
        {
            const Coin ratio = base_row.at( j ) / base_row.at( i );

            // place ratio into commutative row product
            row_product *= ratio;
#ifndef SPRUCE_PERFORMANCE_TWEAKS_ENABLED
            if ( m_visualize )
            {
                if ( i == 0 )
                    m_visualization_rows.clear();

                if ( j == 0 )
                    m_visualized_row_cache.clear();

                // insert space if necessary
                if ( m_visualized_row_cache.isEmpty() )
                    m_visualized_row_cache += QString( "%1 | " ).arg( clist.value( i ), -6 );

                // insert ratio
                m_visualized_row_cache += QString( "%1%2%3  " )
                                   .arg( ratio > CoinAmount::COIN ? COLOR_GREEN :
                                         ratio < CoinAmount::COIN ? COLOR_RED :
                                                                    COLOR_NONE )
                                   .arg( ratio.toString( 2 ), 4 )
                                   .arg( COLOR_NONE );
            }
#endif
        }

        // do base row modulation
        if ( i == base_currency_index )
        {
            for ( QVector<BaseCapitalModulator>::const_iterator j = m_modulator.begin(); j != m_modulator.end(); j++ )
            {
                const BaseCapitalModulator &modulator = *j;

                if ( modulator.ma_slow.getMaxSamples() > 0 &&
                     modulator.ma_slow.getCurrentSamples() == modulator.ma_slow.getMaxSamples() )
                {
                    const Coin threshold = modulator.ma_slow.getSignal() * modulator.ma_slow_threshold;
                    const Coin fast = modulator.ma_fast.getSignal();
                    if ( fast < threshold )
                    {
                        //kDebug() << "bad performance, using modulation" << modulator.ma_slow.getMaxSamples();
                        row_product *= modulator.base_row_modulation * ( threshold / fast );
//                        break; // break after modulation (m_modulator should be bigger modulations first, smaller modulations last)
                    }
                }
            }
        }
#ifndef SPRUCE_PERFORMANCE_TWEAKS_ENABLED
        if ( m_visualize )
        {
            m_visualized_row_cache += QString( "| %1" ).arg( rp.at( rp_idx ).toString( 2 ), -4 );
            m_visualization_rows += m_visualized_row_cache;
        }
#endif
    }

    //kDebug() << "rp:" << rp;

    // for each row product, plot z-value and accumulate total zt
    QVector<Coin> &z = base_row; // reuse base_row
    z.clear();
    Coin zt;
    std::function<Coin(const Coin&)> _alloc_func = m_allocaton_function_vec.at( m_allocation_function_index );
    for ( QVector<Coin>::const_iterator i = rp.begin(); i != rp.end(); i++ )
    {
        // run allocation func
        const Coin &rp = *i;
        const Coin z_value = _alloc_func( rp );

        z += z_value;
        zt += z_value;
    }

    //    kDebug() << "z_score:" << z;
    //    kDebug() << "zt:" << zt;

    // compute z/zt for final allocation
    QVector<Coin> &alloc = rp; // reuse rp
    alloc.clear();
    int &row = i;
    row = 0;
    for ( QVector<Coin>::const_iterator i = z.begin(); i != z.end(); i++ )
    {
        const Coin &z_score = *i;
        const Coin new_alloc = z_score / zt;
        alloc += new_alloc;
#ifndef SPRUCE_PERFORMANCE_TWEAKS_ENABLED
        if ( m_visualize )
        {
            m_visualization_rows[ row ].append( QString( " | %1" )
                                                 .arg( new_alloc.toString( 2 ) ) );

            row++;
        }
#endif
    }

//  const Coin &base_alloc = alloc.at( base_currency_index );
//    kDebug() << alloc.at( base_currency_index );

    // compute new amount for each currency: alloc[ currency ] * base_capital;
    QVector<Coin> &new_base_amount = base_row; // reuse base_row
    new_base_amount.clear();
    for ( QVector<Coin>::const_iterator i = alloc.begin(); i != alloc.end(); i++ )
    {
        const Coin &alloc = *i;
        new_base_amount += alloc * base_capital;
    }

    //kDebug() << "new base amount:" << new_base_amount;

    // compute target qty: m_qty_to_sl = m_currency_qty - ( new_base_amount / m_current_price )
    for ( i = 0; i < new_base_amount.size(); i++ )
    {
        if ( i == base_currency_index )
            continue;

        const QString &currency = clist.at( i );
        const Coin &base = new_base_amount.at( i );
        const Coin &current_price = m_current_price.value( currency );
        const Coin &current_qty = m_current_qty.value( currency );

        m_qty_to_sl[ Market( m_base_currency, currency ) ] = current_qty - ( base / current_price );
    }

    return true;
}

void SpruceV2::doCapitalMomentumModulation( const Coin &base_capital )
{
    // add base capital to fast/slow ma
    for ( QVector<BaseCapitalModulator>::iterator i = m_modulator.begin(); i != m_modulator.end(); i++ )
    {
        (*i).ma_fast.addSample( base_capital );
        (*i).ma_slow.addSample( base_capital );
    }
}

void SpruceV2::adjustCurrentQty( const QString &currency, const Coin &qty )
{
    m_current_qty[ currency ] += qty;
}

QString SpruceV2::getVisualization()
{
    QString ret;
#ifndef SPRUCE_PERFORMANCE_TWEAKS_ENABLED
    for( QVector<QString>::const_iterator i = m_visualization_rows.begin(); i != m_visualization_rows.end(); i++ )
        ret += *i + "\n";
#endif
    return ret;
}

Coin SpruceV2::getBaseCapital()
{
    Coin ret;
    for( QMap<QString, Coin>::const_iterator i = m_current_qty.begin(); i != m_current_qty.end(); i++ )
        if ( i.key() == m_base_currency )
            ret += i.value();
        else
            ret += i.value() * m_current_price.value( i.key() );

    return ret;
}

Coin SpruceV2::getExchangeAllocation( const QString &exchange_market_key )
{
    return per_exchange_market_allocations.value( exchange_market_key );
}

void SpruceV2::setExchangeAllocation( const QString &exchange_market_key, const Coin allocation )
{
    per_exchange_market_allocations.insert( exchange_market_key, allocation );

    kDebug() << "[SpruceV2] exchange allocation for" << exchange_market_key << ":" << allocation;
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

Coin SpruceV2::getOrderNice( const QString &market, const quint8 side, bool midspread_phase )
{
    // get base nice value for side
    const Coin base = ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_buys : m_order_nice_custom_sells :
                                            ( side == SIDE_BUY ) ? m_order_nice_buys : m_order_nice_sells;

    // apply per-market offset to side base
    const Coin base_with_offset = ( side == SIDE_BUY ) ? base + m_order_nice_market_offset_buys.value( market ) :
                                                         base + m_order_nice_market_offset_sells.value( market );

    // apply snapback ratio (or not) to base+offset
    return ( getSnapbackState( market, side ) ) ? base_with_offset * m_snapback_ratio :
                                                  base_with_offset;
}

void SpruceV2::setOrderNiceZeroBound( const quint8 side, Coin nice, bool midspread_phase )
{
    ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys = nice : m_order_nice_custom_zerobound_sells = nice :
                          ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys = nice : m_order_nice_zerobound_sells = nice;
}

Coin SpruceV2::getOrderNiceZeroBound( const QString &market, const quint8 side, bool midspread_phase ) const
{
    const Coin base = ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys : m_order_nice_custom_zerobound_sells :
                                            ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys : m_order_nice_zerobound_sells;

    return ( side == SIDE_BUY ) ? base + m_order_nice_market_offset_zerobound_buys.value( market ) :
                                  base + m_order_nice_market_offset_zerobound_sells.value( market );
}

void SpruceV2::setSnapbackState( const QString &market, const quint8 side, const bool state, const Coin price, const Coin amount_to_shortlong_abs )
{
    // store last trigger2 failure message, but clear when the entire mechanism resets below
    static QMap<QString, qint64> last_trigger2_message;

    const bool opposite_side = ( side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY;
    const bool other_side_state = getSnapbackState( market, opposite_side );

    // if we are turning the state on, and the state of the other side of this market is enabled, disable it directly
    // note: this should almost never happen, as once the amount_to_sl is under the nice limit, snapback  will disable.
    //       if it flips to the other side before disabling, it won't catch the check and we must disable it here.
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
        side == SIDE_BUY ? m_snapback_trigger2_sl_abs_ma_buys[ market ] = Signal( SMA, m_snapback_trigger2_ma_samples ) :
                           m_snapback_trigger2_sl_abs_ma_sells[ market ] = Signal( SMA, m_snapback_trigger2_ma_samples );

        // reset trigger #2 failure message
        last_trigger2_message[ market ] = 0;

        kDebug() << QString( "[Diffusion] %1 Snapback triggers reset for %2" )
                     .arg( description_str )
                     .arg( market );
    }

    // do trigger
    if ( state )
    {
        Signal &amount_to_sl_abs_signal = ( side == SIDE_BUY ) ? m_snapback_trigger2_sl_abs_ma_buys[ market ] :
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
            amount_to_sl_abs_signal.addSample( amount_to_shortlong_abs );

            // trigger2 is finished if:
            // amount to shortlong abs < average of the last SNAPBACK_TRIGGER2_MA_SAMPLES * SNAPBACK_TRIGGER2_MA_RATIO
            // OR
            // we crossed the original trigger #2 price * SNAPBACK_TRIGGER2_PRICE_RATIO;
            QString trigger2_description_str;
            bool trigger2 = false;
            const Coin threshold1 = m_snapback_trigger2_ma_ratio * amount_to_sl_abs_signal.getSignal();
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
    ret += QString( "setspruceordersize %1\n" ).arg( m_order_size );

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

    // save alloc function
    ret += QString( "setsprucealloc %1\n" ).arg( m_allocation_function_index );

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

Coin SpruceV2::getOrderSize( QString market ) const
{
    Q_UNUSED( market )

    return m_order_size;
}

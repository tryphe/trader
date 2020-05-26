#include "spruce.h"
#include "coinamount.h"
#include "global.h"
#include "market.h"

Spruce::Spruce()
{
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
    m_amplification = CoinAmount::COIN;
}

Spruce::~Spruce()
{
    while ( nodes_start.size() > 0 )
        delete nodes_start.takeFirst();

    clearLiveNodes();
}

void Spruce::clear()
{
    Spruce();

    clearLiveNodes();
    clearStartNodes();

    m_currency_profile_u.clear();
    m_currency_reserve.clear();
    m_start_coeffs = m_relative_coeffs = RelativeCoeffs();
    m_quantity_to_shortlong_map.clear();
    original_quantity.clear();
    quantity_already_shortlong.clear();
    quantity_to_shortlong.clear();
    base_currency = QString();
    currency_weight.clear();
    currency_weight_by_coin.clear();
    m_last_coeffs.clear();
    m_qtys.clear();
    m_markets_beta.clear();
}

Coin Spruce::getMarketWeight( QString market ) const
{
    const QList<QString> &currencies = original_quantity.keys();
    for ( QList<QString>::const_iterator i = currencies.begin(); i != currencies.end(); i++ )
    {
        const QString &currency = *i;

        if ( market == Market( base_currency, currency ) )
            return currency_weight.value( currency );
    }

    return Coin();
}

Coin Spruce::getExchangeAllocation( const QString &exchange_market_key )
{
    return per_exchange_market_allocations.value( exchange_market_key );
}

void Spruce::setExchangeAllocation( const QString &exchange_market_key, const Coin allocation )
{
    per_exchange_market_allocations.insert( exchange_market_key, allocation );

    kDebug() << "[Spruce] exchange allocation for" << exchange_market_key << ":" << allocation;
}

Coin Spruce::getOrderGreedRandom( quint8 side ) const
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

void Spruce::setOrderNice( const quint8 side, Coin nice, bool midspread_phase )
{
    ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_buys = nice : m_order_nice_custom_sells = nice :
                          ( side == SIDE_BUY ) ? m_order_nice_buys = nice : m_order_nice_sells = nice;
}

Coin Spruce::getOrderNice( const QString &market, const quint8 side, bool midspread_phase )
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

void Spruce::setOrderNiceZeroBound( const quint8 side, Coin nice, bool midspread_phase )
{
    ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys = nice : m_order_nice_custom_zerobound_sells = nice :
                          ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys = nice : m_order_nice_zerobound_sells = nice;
}

Coin Spruce::getOrderNiceZeroBound( const QString &market, const quint8 side, bool midspread_phase ) const
{
    const Coin base = ( midspread_phase ) ? ( side == SIDE_BUY ) ? m_order_nice_custom_zerobound_buys : m_order_nice_custom_zerobound_sells :
                                            ( side == SIDE_BUY ) ? m_order_nice_zerobound_buys : m_order_nice_zerobound_sells;

    return ( side == SIDE_BUY ) ? base + m_order_nice_market_offset_zerobound_buys.value( market ) :
                                  base + m_order_nice_market_offset_zerobound_sells.value( market );
}

void Spruce::setSnapbackState( const QString &market, const quint8 side, const bool state, const Coin price, const Coin amount_to_shortlong_abs )
{
    // store last trigger2 failure message, but clear when the entire mechanism resets below
    static QMap<QString, qint64> last_trigger2_message;

    const bool opposite_side = ( side == SIDE_BUY ) ? SIDE_SELL : SIDE_BUY;
    const bool other_side_state = getSnapbackState( market, opposite_side );

    // if we are turning the state on, and the state of the other side of this market is enabled, disable it directly
    // note: this should almost never happen realistically, as once the amount_to_sl is under the nice limit, snapback
    //       will disable instantly. however if it flips to the other side, it won't catch the check, and we must disable is here.
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

    // to stop constant enabling/disabling - if we are disabling and our current time quotient matches
    // the last time quotient, don't disable, just return
    if ( !other_side_state && !state && current_time_quotient == trigger1_last_time_quotient )
        return;

    // reset mechanisms if we have a new time quotient, except if we triggered mechanism #2, then just wait for pullback below ma
    if ( state && trigger1_last_time_quotient < current_time_quotient && trigger2_count == 0 )
    {
        // reset trigger #1
        trigger1_last_time_quotient = current_time_quotient;
        trigger1_count = 0;

        // reset trigger #2 ma
        side == SIDE_BUY ? m_snapback_trigger2_sl_abs_ma_buys[ market ] = CoinMovingAverage( m_snapback_trigger2_ma_samples ) :
                           m_snapback_trigger2_sl_abs_ma_sells[ market ] = CoinMovingAverage( m_snapback_trigger2_ma_samples );

        // reset trigger #2 failure message
        last_trigger2_message[ market ] = 0;


        kDebug() << QString( "[Diffusion] %1 Snapback triggers reset for %2" )
                     .arg( description_str )
                     .arg( market );
    }

    // do trigger
    if ( state )
    {
        CoinMovingAverage &amount_to_sl_abs_ma = ( side == SIDE_BUY ) ? m_snapback_trigger2_sl_abs_ma_buys[ market ] :
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
            if ( amount_to_sl_abs_ma.getMaxSamples() != m_snapback_trigger2_ma_samples )
                amount_to_sl_abs_ma.setMaxSamples( m_snapback_trigger2_ma_samples );

            // add sample
            amount_to_sl_abs_ma.addSample( amount_to_shortlong_abs );

            // trigger2 is finished if:
            // amount to shortlong abs < average of the last SNAPBACK_TRIGGER2_MA_SAMPLES * SNAPBACK_TRIGGER2_MA_RATIO
            // OR
            // we crossed the original trigger #2 price * SNAPBACK_TRIGGER2_PRICE_RATIO;
            QString trigger2_description_str;
            bool trigger2 = false;
            const Coin threshold1 = m_snapback_trigger2_ma_ratio * amount_to_sl_abs_ma.getAverage();
            const Coin threshold2 = m_snapback_trigger2_initial_ratio * ( side == SIDE_BUY ?
                                    m_snapback_trigger2_trigger_sl_abs_initial_buys.value( market ) :
                                    m_snapback_trigger2_trigger_sl_abs_initial_sells.value( market ) );

            if ( amount_to_shortlong_abs < threshold1 ||
                 amount_to_shortlong_abs < threshold2  )
            {
                trigger2 = true;
                trigger2_description_str = "crossed either threshold";

                // clear samples so the next time we trigger mechanism #1, #2 trigger2_count check above is zero and we reset both mechanisms
                amount_to_sl_abs_ma.clear();
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
                            .arg( threshold1.toString( 4 ) )
                            .arg( threshold2.toString( 4 ) )
                            .arg( amount_to_shortlong_abs.toString( 4 ) );

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

bool Spruce::getSnapbackState( const QString &market, const quint8 side ) const
{
    return ( side == SIDE_BUY ) ? m_snapback_state_buys.value( market, false ) :
                                  m_snapback_state_sells.value( market, false );
}

void Spruce::addStartNode( QString _currency, QString _quantity, QString _price )
{
    Node *n = new Node();
    n->currency = _currency;
    n->quantity = _quantity;
    n->price = _price;
    n->recalculateAmountByQuantity();

    original_quantity[ _currency ] = _quantity;

    nodes_start += n;
}

void Spruce::addLiveNode( QString _currency, QString _price )
{
    Node *n = new Node();
    n->currency = _currency;
    n->price = _price;

    nodes_now += n;
    nodes_now_by_currency.insert( _currency, n );
}

void Spruce::addMarketBeta( Market m )
{
    if ( !original_quantity.contains( m.getBase() ) ||
         !original_quantity.contains( m.getQuote() ) )
    {
        kDebug() << "spruce error: couldn't find currency" << m.getBase() << "or" << m.getQuote() << "in" << original_quantity;
        return;
    }

    if ( !m_markets_beta.contains( m ) )
        m_markets_beta += m;
}

void Spruce::clearLiveNodes()
{
    while ( nodes_now.size() > 0 )
        delete nodes_now.takeFirst();

    nodes_now_by_currency.clear();
}

void Spruce::clearStartNodes()
{
    while ( nodes_start.size() > 0 )
        delete nodes_start.takeFirst();
}

void Spruce::generateWeights()
{
    currency_weight.clear();
    currency_weight_by_coin.clear();

    QMap<QString, Coin> currency_aps; // sums of amount*price
    Coin largest_aps;

    // calculate aps for each currency
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;

        currency_aps[ n->currency ] = original_quantity.value( n->currency ) * n->price;

        const Coin &current_aps = currency_aps.value( n->currency );
        if ( current_aps > largest_aps )
            largest_aps = current_aps;
    }

    // set weights
    for ( QMap<QString, Coin>::const_iterator i = currency_aps.begin(); i != currency_aps.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &aps = i.value();

        const Coin weight = aps / largest_aps;
        currency_weight[ currency ] = weight;
        currency_weight_by_coin.insert( weight, currency );
    }
}

void Spruce::findOptimalProfiles( const Coin &speculativity, const Coin &error_rate_target, const int max_iterations )
{
    kDebug() << "finding optimal profiles...";

    /// build start node prices
    QMap<QString, Coin> start_node_prices, start_node_qtys, pct_disposed_map;
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        start_node_prices[ (*i)->currency ] = (*i)->price;
        start_node_qtys[ (*i)->currency ] = (*i)->quantity;
    }

    /// run diffusion, but modify the live price of a singular market while using start prices for other markets
    Coin lowest_pct_disposed = CoinAmount::A_LOT, highest_pct_disposed;
    QString highest_pct_disposed_currency, lowest_pct_disposed_currency;
    int iterations = 0;
    while ( lowest_pct_disposed == CoinAmount::A_LOT || // pass on first iteration
            highest_pct_disposed > lowest_pct_disposed * error_rate_target ||
            iterations++ > max_iterations )
    {
        // reset trackers so they set properly each round
        lowest_pct_disposed = CoinAmount::A_LOT;
        highest_pct_disposed = Coin();

        for ( QMap<QString, Coin>::const_iterator i = start_node_prices.begin(); i != start_node_prices.end(); i++ )
        {
            clearLiveNodes();

            const QString &premium_currency = i.key();

            // insert modified price at p == market_modify_index, and for p != market_modify_index, use the original price
            for ( QMap<QString, Coin>::const_iterator j = start_node_prices.begin(); j != start_node_prices.end(); j++ )
            {
                const QString &currency = j.key();
                const Coin &start_node_price = start_node_prices.value( currency );

                if ( currency == premium_currency )
                    addLiveNode( currency, start_node_price * speculativity );
                else
                    addLiveNode( currency, start_node_price );
            }

            calculateAmountToShortLong();

            const Coin &qty_to_sl = m_quantity_to_shortlong_map.value( Market( base_currency, premium_currency ) );
            const Coin pct_disposed = qty_to_sl / start_node_qtys.value( premium_currency ) * 100;
            pct_disposed_map[ premium_currency ] = pct_disposed;

            kDebug() << QString( "currency %1 disposed %2 (%3%)" )
                         .arg( premium_currency, MARKET_STRING_WIDTH )
                         .arg( qty_to_sl, 15 )
                         .arg( pct_disposed.toString( 2 ), 4 );

            // record highest disposed
            if ( pct_disposed > highest_pct_disposed )
            {
                highest_pct_disposed = pct_disposed;
                highest_pct_disposed_currency = premium_currency;
            }

            // record lowest disposed
            if ( pct_disposed < lowest_pct_disposed )
            {
                lowest_pct_disposed = pct_disposed;
                lowest_pct_disposed_currency = premium_currency;
            }
        }

        // raise the highest disposed profile so the performance of other currencies approaches it
        const Coin new_high_profile = getProfileU( highest_pct_disposed_currency ) + Coin( "1" );
        setProfileU( highest_pct_disposed_currency, new_high_profile );

        kDebug() << "profile" << highest_pct_disposed_currency << "=" << new_high_profile;
    }

    kDebug() << "optimized profiles less defaults:" << m_currency_profile_u;
}

bool Spruce::calculateAmountToShortLong()
{
    if ( !normalizeEquity() )
        return false;

    if ( !equalizeDates() )
        return false;

    // record amount to shortlong in a map and get total
    m_quantity_to_shortlong_map.clear();

    QVector<QString> markets = getMarketsAlpha();
    for ( QVector<QString>::const_iterator i = markets.begin(); i != markets.end(); i++ )
    {
        const QString &market = *i;
        const Coin &shortlong_market = getQuantityToShortLongNow( market );

        m_quantity_to_shortlong_map[ market ] = shortlong_market;
    }

    return true;
}

Coin Spruce::getQuantityToShortLongNow( const QString &market )
{
    if ( !quantity_to_shortlong.contains( market ) )
        return Coin();

    Coin ret = -quantity_to_shortlong.value( market ) + quantity_already_shortlong.value( market );

    return ret;
}

void Spruce::addToShortLonged( const QString &market, const Coin &qty )
{
    quantity_already_shortlong[ market ] += qty;
}

QVector<QString> Spruce::getMarketsAlpha() const
{
    QVector<QString> ret;
    const QVector<QString> &keys = original_quantity.keys().toVector();
    for ( QVector<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
        ret += Market( base_currency, *i );

    return ret;
}

bool Spruce::isActive()
{
    return !( nodes_start.isEmpty() || getBaseCurrency() == "disabled" );
}

QString Spruce::getSaveState()
{
    QString ret;

    // save interval
    ret += QString( "setspruceinterval %1\n" ).arg( m_interval_secs );

    // save base
    ret += QString( "setsprucebasecurrency %1\n" ).arg( getBaseCurrency() );

    // save amplification
    ret += QString( "setspruceamplification %1\n" ).arg( m_amplification );

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

    // save profile u
    for ( QMap<QString,Coin>::const_iterator i = m_currency_profile_u.begin(); i != m_currency_profile_u.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &profile_u = i.value();

        // don't save default value
        if ( profile_u == DEFAULT_PROFILE_U )
            continue;

        ret += QString( "setspruceprofile %1 %2\n" )
                .arg( currency )
                .arg( profile_u );
    }

    // save reserve ratio
    for ( QMap<QString,Coin>::const_iterator i = m_currency_reserve.begin(); i != m_currency_reserve.end(); i++ )
    {
        const QString &currency = i.key();
        const Coin &reserve = i.value();

        // don't save default value
        if ( reserve == DEFAULT_RESERVE )
            continue;

        ret += QString( "setsprucereserve %1 %2\n" )
                .arg( currency )
                .arg( reserve );
    }

    // save per-exchange allocations
    for ( QMap<QString,Coin>::const_iterator i = per_exchange_market_allocations.begin(); i != per_exchange_market_allocations.end(); i++ )
    {
        ret += QString( "setspruceallocation %1 %2\n" )
                .arg( i.key() )
                .arg( i.value() );
    }

    // save start nodes
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;

        ret += QString( "setsprucestartnode %1 %2 %3\n" )
                .arg( n->currency )
                .arg( original_quantity.value( n->currency ) )
                .arg( n->price.toSubSatoshiString() );
    }

    // save quantity_already_shortlong
    for ( QMap<QString,Coin>::const_iterator i = quantity_already_shortlong.begin(); i != quantity_already_shortlong.end(); i++ )
    {
        ret += QString( "setspruceshortlongtotal %1 %2\n" )
                .arg( Market( i.key() ) )
                .arg( i.value() );
    }

    // save beta markets
    for ( QList<Market>::const_iterator i = m_markets_beta.begin(); i != m_markets_beta.end(); i++ )
    {
        ret += QString( "setsprucebetamarket %1\n" )
                .arg( *i );
    }

    return ret;
}

Coin Spruce::getOrderSize( QString market ) const
{
    return market.isEmpty() ? m_order_size : std::max( m_order_size * getMarketWeight( market ), getUniversalMinOrderSize() );
}

Coin Spruce::getCurrencyPriceByMarket( Market market )
{
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;

        if ( n->currency == market.getQuote() )
            return n->price;
    }

    return Coin();
}

void Spruce::setProfileU( QString currency, Coin u )
{
    m_currency_profile_u.insert( currency, u );
}

void Spruce::setReserve( QString currency, Coin r )
{
    m_currency_reserve.insert( currency, r );
}

Coin Spruce::getEquityAll()
{
    Coin ret;

    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;

        ret += n->quantity * n->price;
    }

    return ret;
}

Coin Spruce::getLastCoeffForMarket( const QString &market ) const
{
    QString currency = Market( market ).getQuote();

    if ( !m_last_coeffs.contains( currency ) )
        qDebug() << "[Spruce] local warning: can't find coeff for currency" << currency;

    return m_last_coeffs.value( currency );
}

bool Spruce::normalizeEquity()
{
    if ( currency_weight.size() != nodes_start.size() )
        generateWeights();

    if ( nodes_start.size() != nodes_now.size() )
    {
        qDebug() << "[Spruce] local error: spruce: start node count not equal date1 node count";
        return false;
    }

    Coin total, original_total, total_scaled;

    // step 1: calculate total equity
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        total += n->quantity * n->price;
    }
    original_total = total;

    // step 2: calculate mean equity if we were to weight each market the same
    QMap<QString,Coin> mean_equity_for_market;

    Coin mean_equity = total / nodes_start.size();
    mean_equity.truncateByTicksize( CoinAmount::SATOSHI_STR ); // toss subsatoshi digits

    // step 3: calculate weighted equity from lowest to highest weight (map is sorted by weight)
    //         for each market and recalculate mean/total equity
    int ct = nodes_start.size();
    for ( QMultiMap<Coin,QString>::const_iterator i = currency_weight_by_coin.begin(); i != currency_weight_by_coin.end(); i++ )
    {
        const QString &currency = i.value();
        const Coin &weight = i.key();
        const Coin equity_to_use = mean_equity * weight;

        mean_equity_for_market.insert( currency, equity_to_use );

        total_scaled += equity_to_use; // record equity to ensure total_scaled == original total

        // if this is the last item, exit here
        if ( --ct == 0 ) break;

        // do some things to help next iteration, recalculate mean equity based on amount used
        total -= equity_to_use;
        mean_equity = total / ct;
    }

    if ( total_scaled != original_total )
    {
        qDebug() << "[Spruce] local error: spruce: total_scaled != original total (check number of spruce markets)";
        return false;
    }

    // step 4: apply mean equity for each market
    QMap<QString,Coin> start_quantities; // cache date1 quantity to store in date2

    // calculate new equity for all dates: e = mean / price
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        n->amount = mean_equity_for_market.value( n->currency );
        n->recalculateQuantityByPrice();
        start_quantities.insert( n->currency, n->quantity );
    }

    // step 5: put the mean adjusted date1 quantites into date2. after this step, we can figure out the new "normalized" valuations
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;
        n->quantity = start_quantities.value( n->currency );
        n->recalculateAmountByQuantity();
    }

    return true;
}

bool Spruce::equalizeDates()
{
    // ensure dates exist
    if ( nodes_start.size() != nodes_now.size() )
    {
        qDebug() << "[Spruce] local error: couldn't find one of the dates in equalizeDates";
        return false;
    }

    // track shorts/longs
    QMap<QString,Coin> shortlongs;

    // find hi/lo coeffs
    m_start_coeffs = m_relative_coeffs = getRelativeCoeffs();

    static const int MAX_PROBLEM_PARTS = 10000;
    static const Coin MIN_TICKSIZE = CoinAmount::SATOSHI * 50000;
    const Coin equity = getEquityAll();
    const Coin ticksize = std::max( MIN_TICKSIZE, equity / MAX_PROBLEM_PARTS );
    const Coin ticksize_amplified = ticksize * m_amplification;

    // if we don't have enough to make the adjustment, abort
    if ( equity < getUniversalMinOrderSize() )
    {
        kDebug() << "[Spruce] local warning: not enough equity to equalizeDates" << equity;
        return false;
    }

    // run divide-and-conquer algorithm which approaches an optimal portfolio according to
    // a per-market cost function.

    /// psuedocode
    //
    // get initial coeffs
    // find hi/lo
    // while ( more is left to short/long )
    //     short highest coeff market
    //     long lowest coeff market
    //     get new market coeffs, set new hi/lo
    ///

    quint16 i = 0;
    while ( true )
    {
        Node *node_long  = nodes_now_by_currency.value( m_relative_coeffs.lo_currency ),
             *node_short = nodes_now_by_currency.value( m_relative_coeffs.hi_currency );

        // short highest coeff, long lowest coeff
        if ( node_long && node_short )
        {
            Coin qty_short = ( ticksize_amplified / node_short->price ),
                 qty_long  = ( ticksize_amplified / node_long->price );

            if ( node_short->quantity > qty_short )
            {
                shortlongs[ node_short->currency ] -= qty_short;
                shortlongs[ node_long->currency  ] += qty_long;

                node_short->amount -= ticksize;
                node_long->amount += ticksize;

                node_short->recalculateQuantityByPrice();
                node_long->recalculateQuantityByPrice();
            }
        }

        // get new coeffs so we can analyze m_qtys
        m_relative_coeffs = getRelativeCoeffs();

        // break on consistent sawtooth pattern, which means we're done!
        if ( m_qtys.size() >= nodes_now_by_currency.size() &&
             ( m_qtys.value( 0 ) == m_qtys.value( m_qtys.value( 0 ).size() -1 ) ||
               m_qtys.value( 0 ) == m_qtys.value( m_qtys.value( 0 ).size() -2 ) ) )
            break;

        // break at equity limit to avoid infinite loop or bad things
        if ( i++ == MAX_PROBLEM_PARTS )
            break;
    }

    // put shortlongs into qty_to_shortlong with market name as key
    for ( QMap<QString,Coin>::const_iterator i = shortlongs.begin(); i != shortlongs.end(); i++ )
        quantity_to_shortlong[ Market( base_currency, i.key() ) ] = i.value();

    return true;
}

RelativeCoeffs Spruce::getRelativeCoeffs()
{
    // get coeffs for time distances of balances
    m_last_coeffs = getMarketCoeffs();

    // find the highest and lowest coefficents
    RelativeCoeffs ret;
    QMap<QString/*currency*/,Coin> qtys;
    QMap<QString,Coin>::const_iterator begin = m_last_coeffs.begin(),
                                       end = m_last_coeffs.end();
    for ( QMap<QString,Coin>::const_iterator i = begin; i != end; i++ )
    {
        const QString &currency = i.key();
        const Coin &coeff = i.value();

        if ( coeff > ret.hi_coeff )
        {
            ret.hi_coeff  = coeff;
            ret.hi_currency = currency;
        }

        if ( coeff < ret.lo_coeff )
        {
            ret.lo_coeff  = coeff;
            ret.lo_currency = currency;
        }

        // build qtys
        qtys.insert( currency, nodes_now_by_currency.value( currency )->quantity );
    }

    m_qtys.prepend( qtys );

    // remove cache beyond number of currencies
    if ( m_qtys.size() > m_last_coeffs.size() )
        m_qtys.removeLast();

    return ret;
}

QMap<QString, Coin> Spruce::getMarketCoeffs()
{
    QMap<QString/*currency*/,Coin> start_scores, relative_coeff;

    // calculate start scores
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        start_scores.insert( n->currency, n->quantity * n->price );
    }

    // calculate new score based on starting score using a loss function
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;
        const Coin score = n->quantity * n->price;
        const Coin &start_score = start_scores.value( n->currency );
        Coin &new_coeff = relative_coeff[ n->currency ];

        // obtain a ratio between 0 and m_log_map_end
        bool is_negative = score < start_score;
        Coin normalized_score = is_negative ? start_score / score
                                            : score / start_score;

        // find a granular point so we can map our ratio to a point in the image
        normalized_score.truncateByTicksize( m_cost_cache.getTicksize() );
        normalized_score -= CoinAmount::COIN; // subtract 1, the origin

        // clamp score above maximum
        const Coin &max_x = m_cost_cache.getMaxX();
        if ( normalized_score > max_x )
            normalized_score = max_x;

        // translate the normalized score with the cost function
        normalized_score = m_cost_cache.getY( m_currency_profile_u.value( n->currency, DEFAULT_PROFILE_U ),
                                              m_currency_reserve.value( n->currency, DEFAULT_RESERVE ),
                                              normalized_score );

        // if we are negative, since f(x) == f(-x), we don't store negative values.
        // apply reflection -f(x) instead of running f(-x)
        if ( is_negative )
            normalized_score = -normalized_score;

        // set new coeff
        new_coeff = normalized_score;
    }

    return relative_coeff;
}

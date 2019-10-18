#include "spruce.h"
#include "coinamount.h"
#include "global.h"
#include "market.h"

#include <QRandomGenerator>

Spruce::Spruce()
{
    /// user settings
    m_order_size = "0.00500000";
    m_order_nice = "2";
    m_order_nice_spreadput = "30";
    m_order_greed = "0.99"; // keep our spread at least 1-x% apart
    m_order_greed_randomness = "0.005"; // randomly subtract tenths of a pct from greed up to this amount

    m_long_max = "0.3000000"; // max long total
    m_short_max = "-0.50000000"; // max short total
    m_market_buy_max = "0.20000000";
    m_market_sell_max = "0.20000000";
    m_trailing_price_limit = "0.96";
    m_leverage = CoinAmount::COIN;
}

Spruce::~Spruce()
{
    while ( nodes_start.size() > 0 )
        delete nodes_start.takeFirst();

    clearLiveNodes();
}

void Spruce::setCurrencyWeight( QString currency, Coin weight )
{
    // clear by coin
    for ( QMultiMap<Coin,QString>::const_iterator i = currency_weight_by_coin.begin(); i != currency_weight_by_coin.end(); i++ )
    {
        if ( i.value() == currency )
        {
            currency_weight_by_coin.remove( i.key(), i.value() );
            break;
        }
    }

    currency_weight[ currency ] = weight;
    currency_weight_by_coin.insert( weight, currency );
}

Coin Spruce::getMarketWeight( QString market ) const
{
    const QList<QString> &currencies = getCurrencies();
    for ( QList<QString>::const_iterator i = currencies.begin(); i != currencies.end(); i++ )
    {
        const QString &currency = *i;

        if ( market == Market( base_currency, currency ) )
            return currency_weight.value( currency );
    }

    return Coin();
}

Coin Spruce::getOrderGreed()
{
    // if greed is unset, just return m_order_greed
    if ( !m_order_greed.isGreaterThanZero() )
        return m_order_greed;

    // try to generate rand range
    const Coin iter = CoinAmount::COIN / 1000;
    const quint32 range = ( m_order_greed_randomness / iter ).toUInt32();

    // if the range is 0, return here to prevent a range of 0-1
    if ( range == 0 )
        return 0;

    const quint32 rand = QRandomGenerator::global()->generate() % ( range +1 );
    const Coin ret = m_order_greed - ( iter * rand );

    // don't return negative greed value
    return ret.isLessThanZero() ? Coin() : ret;
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

void Spruce::clearLiveNodes()
{
    while ( nodes_now.size() > 0 )
        delete nodes_now.takeFirst();

    nodes_now_by_currency.clear();
}

bool Spruce::calculateAmountToShortLong()
{
    if ( !normalizeEquity() )
        return false;

    if ( !equalizeDates() )
        return false;

    // record amount to shortlong in a map and get total
    m_quantity_to_shortlong_map.clear();

    QList<QString> markets = getMarkets();
    for ( QList<QString>::const_iterator i = markets.begin(); i != markets.end(); i++ )
    {
        const QString &market = *i;
        const Coin &shortlong_market = getQuantityToShortLongNow( market );

        m_quantity_to_shortlong_map[ market ] = shortlong_market;
    }

    return true;
}

Coin Spruce::getQuantityToShortLongNow( QString market )
{
    if ( !quantity_to_shortlong.contains( market ) )
        return Coin();

    Coin ret = -quantity_to_shortlong.value( market );

    return ret;
}

void Spruce::addToShortLonged( QString market, Coin qty )
{
    quantity_already_shortlong[ market ] += qty;
}

QList<QString> Spruce::getCurrencies() const
{
    return original_quantity.keys();
}

QList<QString> Spruce::getMarkets() const
{
    QList<QString> ret;
    const QList<QString> &keys = original_quantity.keys();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
        ret += Market( base_currency, *i );

    return ret;
}

bool Spruce::isActive()
{
    return !( base_currency.isEmpty() || nodes_start.isEmpty() || currency_weight.isEmpty() || base_currency == "disabled" );
}

QString Spruce::getSaveState()
{
    QString ret;

    // save base
    ret += QString( "setsprucebasecurrency %1\n" ).arg( base_currency.isEmpty() ? "disabled" : base_currency );

    // save log factor
    ret += QString( "setspruceleverage %1\n" ).arg( m_leverage );

    // save order greed
    ret += QString( "setspruceordergreed %1 %2\n" )
            .arg( m_order_greed )
            .arg( m_order_greed_randomness );

    // save long max
    ret += QString( "setsprucelongmax %1\n" ).arg( m_long_max );

    // save short max
    ret += QString( "setspruceshortmax %1\n" ).arg( m_short_max );

    // save market max
    ret += QString( "setsprucemarketmax %1 %2\n" ).arg( m_market_buy_max )
                                                  .arg( m_market_sell_max );

    // save order size
    ret += QString( "setspruceordersize %1\n" ).arg( m_order_size );

    // save order size
    ret += QString( "setspruceordernice %1 %2\n" ).arg( m_order_nice )
                                                  .arg( m_order_nice_spreadput );

    // save order trailing limit
    ret += QString( "setspruceordertrail %1\n" ).arg( m_trailing_price_limit );

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

    // save market weights
    for ( QMap<QString,Coin>::const_iterator i = currency_weight.begin(); i != currency_weight.end(); i++ )
    {
        ret += QString( "setspruceweight %1 %2\n" )
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

    return ret;
}

Coin Spruce::getMarketBuyMax( QString market ) const
{
    return market.isEmpty() ? m_market_buy_max : std::max( m_market_buy_max * getMarketWeight( market ), m_market_buy_max * Coin( "0.1" ) );
}

Coin Spruce::getMarketSellMax( QString market ) const
{
    return market.isEmpty() ? m_market_sell_max : std::max( m_market_sell_max * getMarketWeight( market ), m_market_sell_max * Coin( "0.1" ) );

}
Coin Spruce::getOrderSize( QString market ) const
{
    return market.isEmpty() ? m_order_size : std::max( m_order_size * getMarketWeight( market ), Coin( MINIMUM_ORDER_SIZE ) );
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

Coin Spruce::getEquityNow( QString currency )
{
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;

        if ( n->currency == currency )
            return n->quantity * n->price;
    }

    return Coin();
}

Coin Spruce::getLastCoeffForMarket( const QString &market ) const
{
    QString currency = Market( market ).getQuote();

    if ( !m_coeffs.value( 0 ).contains( currency ) )
        qDebug() << "[Spruce] local warning: can't find coeff for currency" << currency;

    return m_coeffs.value( 0 ).value( currency );
}

bool Spruce::normalizeEquity()
{
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
    mean_equity.truncateByTicksize( "0.00000001" ); // toss subsatoshi digits

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
        n->quantity = start_quantities.value( n->currency ) + quantity_already_shortlong.value( Market( base_currency, n->currency ) );
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

    static const int MAX_PROBLEM_PARTS = 15000;
    static const Coin MIN_TICKSIZE = CoinAmount::SATOSHI * 60000;
    const Coin hi_equity = getEquityNow( m_relative_coeffs.hi_currency );
    const Coin ticksize = std::max( MIN_TICKSIZE, hi_equity / MAX_PROBLEM_PARTS );
    const Coin ticksize_leveraged = ticksize * m_leverage;

    // if we don't have enough to make the adjustment, abort
    if ( hi_equity < MINIMUM_ORDER_SIZE )
    {
        kDebug() << "[Spruce] local warning: not enough equity to equalizeDates" << hi_equity;
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

        Coin qty_short = ( ticksize_leveraged / node_short->price ),
             qty_long  = ( ticksize_leveraged / node_long->price );

        // short highest coeff, long lowest coeff
        if ( node_long && node_short &&
             node_short->quantity > qty_short )
        {
            shortlongs[ node_short->currency ] -= qty_short;
            shortlongs[ node_long->currency  ] += qty_long;

            node_short->amount -= ticksize;
            node_long->amount += ticksize;

            node_short->recalculateQuantityByPrice();
            node_long->recalculateQuantityByPrice();
        }

        // get new coeffs so we can analyze m_coeffs
        m_relative_coeffs = getRelativeCoeffs();

        // break on consistent sawtooth pattern, which means we're done!
        if ( m_coeffs.value( 0 ) == m_coeffs.value( m_coeffs.value( 0 ).size() -1 ) ||
             m_coeffs.value( 0 ) == m_coeffs.value( m_coeffs.value( 0 ).size() -2 ) )
            break;

        // break at max equity to avoid infinite loop
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
    m_coeffs.prepend( getMarketCoeffs() );

    // remove cache beyond number of markets
    if ( m_coeffs.size() > m_coeffs.at( 0 ).size() )
        m_coeffs.removeLast();

    // find the highest and lowest coefficents
    RelativeCoeffs ret;
    QMap<QString,Coin>::const_iterator begin = m_coeffs.at( 0 ).begin(),
                                       end = m_coeffs.at( 0 ).end();
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
    }

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

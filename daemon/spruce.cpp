#include "spruce.h"
#include "coinamount.h"
#include <global.h>

Spruce::Spruce()
{
    /// user settings
    m_leverage = m_default_leverage = "0.5";
    m_hedge_target = "0.95"; // keep our market valuations at most 1-x% apart
    m_order_greed = "0.99"; // keep our spread at least 1-x% apart

    m_long_max = "0.3000000"; // max long total
    m_short_max = "-0.50000000"; // max short total
    m_market_max = "0.20000000";
    m_order_size = "0.00500000";
    m_order_nice = "2";

    /// per-exchange constants
    m_order_size_min = "0.00070000"; // TODO: scale this minimum to each exchange
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
    for ( QList<QString>::const_iterator i = getCurrencies().begin(); i != getCurrencies().end(); i++ )
    {
        const QString &currency = *i;
        const QString market_recreated = getBaseCurrency() + "-" + currency;

        if ( market == market_recreated )
            return currency_weight.value( currency );
    }

    return Coin();
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
}

void Spruce::clearLiveNodes()
{
    while ( nodes_now.size() > 0 )
        delete nodes_now.takeFirst();
}

void Spruce::calculateAmountToShortLong()
{
    normalizeEquity();
    equalizeDates();

    // record amount to shortlong in a map and get total
    m_amount_to_shortlong_map.clear();
    m_amount_to_shortlong_total = Coin();

    QList<QString> markets = getMarkets();
    for ( QList<QString>::const_iterator i = markets.begin(); i != markets.end(); i++ )
    {
        const QString &market = *i;
        const Coin &shortlong_market = getAmountToShortLongNow( market );

        m_amount_to_shortlong_map[ market ] = shortlong_market;
        m_amount_to_shortlong_total += shortlong_market;
    }
}

Coin Spruce::getAmountToShortLongNow( QString market )
{
    if ( !amount_to_shortlong.contains( market ) )
        return Coin();

    Coin ret = -amount_to_shortlong.value( market ) + shortlonged_total.value( market );

    return ret;
}

void Spruce::addToShortLonged( QString market, Coin amount )
{
    shortlonged_total[ market ] += amount;
}

QList<QString> Spruce::getCurrencies() const
{
    return original_quantity.keys();
}

QList<QString> Spruce::getMarkets() const
{
    // TODO: adapt this to each exchange
    QList<QString> ret;
    const QList<QString> &keys = original_quantity.keys();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
        ret += base_currency + "-" + *i;

    return ret;
}

QString Spruce::getSaveState()
{
    QString ret;

    if ( !isActive() )
        return ret;

    // save base
    ret += QString( "setsprucebasecurrency %1\n" ).arg( base_currency );

    // save leverage
    ret += QString( "setspruceleverage %1\n" ).arg( m_default_leverage );

    // save leverage cutoff
    for ( QMap<Coin,Coin>::const_iterator i = m_leverage_cutoff.begin(); i != m_leverage_cutoff.end(); i++ )
    {
        ret += QString( "setspruceleveragecutoff %1 %2\n" )
                .arg( i.key() )
                .arg( i.value() );
    }

    // save hedge target
    ret += QString( "setsprucehedgetarget %1\n" ).arg( m_hedge_target );

    // save order greed
    ret += QString( "setspruceordergreed %1\n" ).arg( m_order_greed );

    // save long max
    ret += QString( "setsprucelongmax %1\n" ).arg( m_long_max );

    // save short max
    ret += QString( "setspruceshortmax %1\n" ).arg( m_short_max );

    // save market max
    ret += QString( "setsprucemarketmax %1\n" ).arg( m_market_max );

    // save order size
    ret += QString( "setspruceordersize %1\n" ).arg( m_order_size );

    // save order size
    ret += QString( "setspruceordernice %1\n" ).arg( m_order_nice );


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
                .arg( n->price );
    }

    // save shortlonged_total
    for ( QMap<QString,Coin>::const_iterator i = shortlonged_total.begin(); i != shortlonged_total.end(); i++ )
    {
        ret += QString( "setspruceshortlongtotal %1 %2\n" )
                .arg( i.key() )
                .arg( i.value() );
    }

    return ret;
}

void Spruce::equalizeDates()
{
    /// psuedocode
    //
    // get initial coeffs
    // find hi/lo
    // while hi.ratio(0.99) > lo
    //     shortlongs[ highest coeff market ] -= 100k sat
    //     shortlongs[ lowest coeff market ] += 100k sat
    //     get new coeff, set new hi/lo
    ///

    // ensure dates exist
    if ( nodes_start.size() != nodes_now.size() )
    {
        qDebug() << "error: couldn't find one of the dates";
        return;
    }

    // track shorts/longs
    QMap<QString,Coin> shortlongs;

    // find hi/lo coeffs
    m_start_coeffs = m_relative_coeffs = getRelativeCoeffs();

    Coin ticksize = CoinAmount::SATOSHI * 50000;

    // avoid infinite loop
    if ( m_hedge_target > Coin( "0.998" ) )
        m_hedge_target = "0.998";

    // check for bad coeff
    if ( m_start_coeffs.lo_coeff.isZeroOrLess() )
        return;

    // find leverage to use by reverse iterating from highest cutoff to lowest
    bool cutoff_tripped = false;
    const Coin coeff_diff_pct = m_start_coeffs.hi_coeff / m_start_coeffs.lo_coeff;
    for ( QMap<Coin,Coin>::const_iterator i = m_leverage_cutoff.end() -1; i != m_leverage_cutoff.begin() -1; i-- )
    {
        const Coin &cutoff = i.key();
        const Coin &new_leverage = i.value();

        if ( coeff_diff_pct >= cutoff &&
             m_leverage > new_leverage )
        {
            cutoff_tripped = true;
            m_leverage = new_leverage;
            kDebug() << "[Spruce] leverage lowered to" << m_leverage;
            break;
        }
    }

    // if we didn't trip the cutoff, set it to default
    if ( !cutoff_tripped && m_leverage != m_default_leverage )
    {
        m_leverage = m_default_leverage;
        kDebug() << "[Spruce] leverage reset to" << m_leverage;
    }

    quint64 ct = 1;
    while ( m_relative_coeffs.hi_coeff * m_hedge_target > m_relative_coeffs.lo_coeff )
    {
        // if we are dealing with a lot of btc, speed up the ratio convergence
        if ( ct++ % 10000 == 0 )
            ticksize += CoinAmount::SATOSHI * 50000;

        // find highest/lowest coeff market
        for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
        {
            Node *n = *i;

            if ( n->currency == m_relative_coeffs.hi_currency &&
                 n->amount > ticksize ) // check if we have enough to short
            {
                shortlongs[ n->currency ] -= ticksize * m_leverage;
                n->amount -= ticksize;
            }
            else if ( n->currency == m_relative_coeffs.lo_currency )
            {
                shortlongs[ n->currency ] += ticksize * m_leverage;
                n->amount += ticksize;
            }
            else
            {
                continue;
            }

            n->recalculateQuantityByPrice();
        }

        m_relative_coeffs = getRelativeCoeffs();
    }

    // flip values, because we want shorts as positive and longs as negative
    for ( QMap<QString,Coin>::const_iterator i = shortlongs.begin(); i != shortlongs.end(); i++ )
    {
        QString market = i.key();

        // TODO: adapt this to each exchange
        market.prepend( base_currency + "-" );

        amount_to_shortlong[ market ] = i.value();
    }
}

void Spruce::normalizeEquity()
{
    if ( nodes_start.size() != nodes_now.size() )
    {
        qDebug() << "local error: spruce: start node count not equal date1 node count";
        return;
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
    //qDebug() << "starting equity:" << total << "mean:" << mean_equity;

    // step 3: calculate weighted equity from lowest to highest weight (multimap is sorted by weight)
    //         for each market and recalculate mean/total equity
    int ct = nodes_start.size();
    for ( QMultiMap<Coin,QString>::const_iterator i = currency_weight_by_coin.begin(); i != currency_weight_by_coin.end(); i++ )
    {
        const QString &currency = i.value();
        const Coin &weight = i.key();

        Coin equity_to_use = mean_equity * weight;
        mean_equity_for_market.insert( currency, equity_to_use );

        //qDebug() << "equity scaled for" << currency << equity_to_use;
        total_scaled += equity_to_use; // record equity to ensure total_scaled == original total

        // avoid div0 on last iteration
        if ( --ct == 0 )
            break;

        // recalculate mean based on amount used
        total -= equity_to_use;
        mean_equity = total / ct;
    }
    //assert( total_scaled == original_total );

    if ( total_scaled != original_total )
    {
        qDebug() << "local error: spruce: total_scaled != original total (check number of spruce markets)";
        return;
    }

//    qDebug() << "equity used:" << total_scaled;
//    qDebug() << "equity available:" << original_total;

    // step 4: apply mean equity for each market
    QMap<QString,Coin> start_quantities; // cache date1 quantity to store in date2

    // calculate new equity for all dates: e = mean / price
    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        n->amount = mean_equity_for_market.value( n->currency, Coin(1) );
        n->recalculateQuantityByPrice();
        start_quantities.insert( n->currency, n->quantity );
        //qDebug() << n->currency << "quantity is now" << n->quantity;
    }

    // step 5: put the mean adjusted date1 quantites into date2. after this step, we can figure out the new "normalized" valuations
    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;
        n->quantity = start_quantities.value( n->currency );
        n->recalculateAmountByQuantity();
        //qDebug() << n->currency << "quantity is now" << n->quantity;
    }
}

QMap<QString, Coin> Spruce::getMarketCoeffs()
{
    QMap<QString/*currency*/,Coin> start_scores, relative_coeff;

    for ( QList<Node*>::const_iterator i = nodes_start.begin(); i != nodes_start.end(); i++ )
    {
        Node *n = *i;
        Coin score = n->quantity * n->price;

        start_scores.insert( n->currency, score );
    }

    for ( QList<Node*>::const_iterator i = nodes_now.begin(); i != nodes_now.end(); i++ )
    {
        Node *n = *i;
        Coin score = n->quantity * n->price;

        // coeff[n] = date2[n] / date1[n]
        relative_coeff[ n->currency ] = score / start_scores.value( n->currency );
    }

    return relative_coeff;
}

RelativeCoeffs Spruce::getRelativeCoeffs()
{
    // get coeffs for time distances of balances
    QMap<QString/*currency*/,Coin> coeffs = getMarketCoeffs();

    // find the highest and lowest coefficents
    RelativeCoeffs ret;
    for ( QMap<QString,Coin>::const_iterator i = coeffs.begin(); i != coeffs.end(); i++ )
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

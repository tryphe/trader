#include "spruce.h"
#include "coinamount.h"

Spruce::Spruce()
{
}

Spruce::~Spruce()
{
    while ( nodes_start.size() > 0 )
        delete nodes_start.takeFirst();

    clearLiveNodes();
}

Coin Spruce::getMarketWeight( QString currency )
{
    QString market = getBaseCurrency() + "-" + currency;

    if ( !market_weight.values().contains( currency ) )
        return Coin();

    Coin ret;
    for ( QMultiMap<Coin,QString>::const_iterator i = market_weight.begin(); i != market_weight.end(); i++ )
    {
        if ( i.value() == currency )
        {
            ret = i.key();
            break;
        }
    }

    return ret;
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
}

Coin Spruce::getAmountToShortLongNow( QString market )
{
    if ( !amount_to_shortlong.contains( market ) )
        return Coin();

    Coin ret = ( Coin() - amount_to_shortlong.value( market ) ) + shortlonged_total.value( market );

    return ret;
}

void Spruce::addToShortLonged( QString market, Coin amount )
{
    shortlonged_total[ market ] += amount;
}

QList<QString> Spruce::getCurrencies()
{
    return original_quantity.keys();
}

QList<QString> Spruce::getMarkets()
{
    QList<QString> keys = original_quantity.keys(), ret;

    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
    {
        QString market = *i; // get currency
        // TODO: adapt this to each exchange
        market.prepend( base_currency + "-" );
        ret += market;
    }

    return ret;
}

QString Spruce::getSaveState()
{
    QString ret;

    if ( !isActive() )
        return ret;

    // save base
    ret += QString( "setsprucebasecurrency %1\n" )
            .arg( base_currency );

    // save market weights
    for ( QMultiMap<Coin,QString>::const_iterator i = market_weight.begin(); i != market_weight.end(); i++ )
    {
        ret += QString( "setspruceweight %1 %2\n" )
                .arg( i.value() )
                .arg( i.key() );
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

    // get coeffs for time distances of balances
    QMap<QString/*currency*/,Coin> coeffs = getMarketCoeffs();

    // find hi/lo coeffs
    RelativeCoeffs relative = getHiLoCoeffs( coeffs );

    Coin ticksize = Coin( "0.00100000" );
    Coin leverage = Coin( "1.40" ); // not really leverage, just for scaling

    QList<Node*> &new_nodes = nodes_now;
    while ( relative.hi_coeff.ratio( 0.97 ) > relative.lo_coeff )
    {
        // find highest/lowest coeff market
        for ( QList<Node*>::const_iterator i = new_nodes.begin(); i != new_nodes.end(); i++ )
        {
            Node *n = *i;

            if ( n->currency == relative.hi_currency &&
                 n->amount > ticksize *10 ) // check if we have enough to short
            {
                //qDebug() << n->currency << "has coeff" << relative.hi_coeff << ", shorting" << pip_size;
                shortlongs[ n->currency ] -= ticksize * leverage;
                n->amount -= ticksize;
            }
            else if ( n->currency == relative.lo_currency )
            {
                //qDebug() << n->currency << "has coeff" << relative.lo_coeff << ", longing" << pip_size;
                shortlongs[ n->currency ] += ticksize * leverage;
                n->amount += ticksize;
            }
            else
            {
                continue;
            }

            //n->quantity *= Coin( old_amount / n->amount ); // adjust the quantity by modified amount ratio
            n->recalculateQuantityByPrice();
        }

        coeffs = getMarketCoeffs();
        relative = getHiLoCoeffs( coeffs );
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
    for ( QMultiMap<Coin,QString>::const_iterator i = market_weight.begin(); i != market_weight.end(); i++ )
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
    QMap<QString/*currency*/,Coin> start_scores, date1_scores;
    QMap<QString/*currency*/,Coin> relative_coeff;

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
        date1_scores.insert( n->currency, score );

        // coeff[n] = date2[n] / date1[n]
        relative_coeff[ n->currency ] = score / start_scores.value( n->currency );
    }

    return relative_coeff;
}

RelativeCoeffs Spruce::getHiLoCoeffs( QMap<QString, Coin> &coeffs )
{
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

#include "alphatracker.h"
#include "position.h"
#include "global.h"

AlphaTracker::AlphaTracker()
{
}

void AlphaTracker::addAlpha( const QString &market, Position *pos )
{
    QMap<QString,AlphaData> &map = pos->side == SIDE_BUY ? buys : sells;
    AlphaData &d = map[ market ];

    // for each trade, v += volume, and vp += volume * price
    d.v += pos->btc_amount;
    d.vp += pos->btc_amount * pos->price;
    d.trades++;
}

void AlphaTracker::reset()
{
    buys.clear();
    sells.clear();
}

Coin AlphaTracker::getAlpha( const QString &market ) const
{
    const AlphaData &sell_data = sells.value( market );
    const AlphaData &buy_data = buys.value( market );

    if ( sell_data.trades == 0 || buy_data.trades == 0 )
        return Coin();

    return ( getAvgPrice( market, SIDE_SELL ) / getAvgPrice( market, SIDE_BUY ) );
}

Coin AlphaTracker::getVolume( const QString &market ) const
{
    return buys.value( market ).v + sells.value( market ).v;
}

Coin AlphaTracker::getVolumePerTrade( const QString &market ) const
{
    const quint64 t = getTrades( market );

    if ( t == 0 )
        return Coin();

    return getVolume( market ) / t;
}

Coin AlphaTracker::getAvgPrice( const QString &market, quint8 side ) const
{
    if ( side == SIDE_BUY )
    {
        const AlphaData &buy_data = buys.value( market );
        return buy_data.trades == 0 ? Coin() : buy_data.vp / buy_data.v;
    }

    const AlphaData &sell_data = sells.value( market );
    return sell_data.trades == 0 ? Coin() : sell_data.vp / sell_data.v;
}

quint64 AlphaTracker::getTrades( const QString &market ) const
{
    return buys.value( market ).trades + sells.value( market ).trades;
}

void AlphaTracker::printAlpha() const
{
    const QList<QString> keys = getMarkets();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
    {
        const QString &market = *i;
        kDebug() << QString( "%1 | alpha %2 | buy %3 | sell %4 | vol %5 | vol-trade %6 | trades %7" )
                    .arg( market, -MARKET_STRING_WIDTH )
                    .arg( getAlpha( market ), -10 )
                    .arg( getAvgPrice( market, SIDE_BUY ), -12 )
                    .arg( getAvgPrice( market, SIDE_SELL ), -12 )
                    .arg( getVolume( market ), -12 )
                    .arg( getVolumePerTrade( market ), -12 )
                    .arg( getTrades( market ), -7 );

    }
}

QString AlphaTracker::getSaveState() const
{
    QString ret;
    const QList<QString> keys = getMarkets();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
    {
        const QString &market = *i;

        for( quint8 side = SIDE_BUY; side < SIDE_SELL +1; side++ )
        {
            const QMap<QString,AlphaData> &map = ( side == SIDE_BUY ) ? buys : sells;
            const AlphaData &d = map.value( market );

            // don't save alpha without some trades
            if ( d.trades == 0 )
                continue;

            ret += QString( "a %1 %2 %3 %4 %5\n" )
                        .arg( market )
                        .arg( side )
                        .arg( d.v.toSubSatoshiString() )
                        .arg( d.vp.toSubSatoshiString() )
                        .arg( d.trades );
        }
    }

    return ret;
}

void AlphaTracker::readSaveState( const QString &state )
{
    QList<QString> lines = state.split( QChar( '\n' ) );

    for ( int i = 0; i < lines.size(); i++ )
    {
        const QString &line = lines.value( i );
        QList<QString> args = line.split( QChar( ' ' ) );

        if ( args.size() < 6 || args.value( 0 ) != "a" )
            continue;

        // read market and side
        const QString &market = args.at( 1 );
        quint8 side = args.at( 2 ).toUShort();

        QMap<QString,AlphaData> &map = ( side == SIDE_BUY ) ? buys : sells;
        AlphaData &d = map[ market ];

        // read alpha values
        d.v = args.at( 3 );
        d.vp = args.at( 4 );
        d.trades = args.at( 5 ).toULongLong();
    }
}

QList<QString> AlphaTracker::getMarkets() const
{
    // put the keys of buys and sells into a qstringlist
    QList<QString> ret;
    for( quint8 side = SIDE_BUY; side < SIDE_SELL +1; side++ )
    {
        const QMap<QString,AlphaData> &map = side == SIDE_BUY ? buys : sells;
        const QList<QString> &keys = map.keys();

        for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
            if ( !ret.contains( *i ) )
                ret.append( *i );
    }

    // return sorted list
    std::sort( ret.begin(), ret.end() );
    return ret;
}

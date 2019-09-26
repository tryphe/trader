#include "alphatracker.h"
#include "position.h"
#include "global.h"

AlphaTracker::AlphaTracker()
{
}

Coin AlphaTracker::getAlpha( const QString &market )
{
    AlphaData &sell_data = sells[ market ];
    AlphaData &buy_data = buys[ market ];

    if ( buy_data.trades == 0 )
        return Coin();

    return ( sell_data.getAvgPrice() / buy_data.getAvgPrice() );
}

Coin AlphaTracker::getVolume( const QString &market )
{
    return buys[ market ].v + sells[ market ].v;
}

Coin AlphaTracker::getVolumePerTrade( const QString &market )
{
    const quint64 t = getTrades( market );

    if ( t == 0 )
        return Coin();

    return getVolume( market ) / t;
}

Coin AlphaTracker::getAvgPrice( const QString &market, quint8 side )
{
    if ( side == SIDE_BUY )
    {
        AlphaData &buy_data = buys[ market ];
        return buy_data.vp / buy_data.v;
    }

    AlphaData &sell_data = sells[ market ];
    return sell_data.vp / sell_data.v;
}

quint64 AlphaTracker::getTrades( const QString &market )
{
    return buys[ market ].trades + sells[ market ].trades;
}

void AlphaTracker::addAlpha( const QString &market, Position *pos )
{
    QMap<QString,AlphaData> &map = pos->side == SIDE_BUY ? buys : sells;
    AlphaData &d = map[ market ];

    // for each trade, v += volume * price, and v += volume
    d.v += pos->btc_amount;
    d.vp += pos->btc_amount * pos->price;
    d.trades++;
}

void AlphaTracker::reset()
{
    buys.clear();
    sells.clear();
}

void AlphaTracker::printAlpha()
{
    const QList<QString> &keys = buys.keys();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
    {
        const QString &market = *i;
        kDebug() << QString( "%1 | alpha %2 | avg_buy %3 | avg_sell %4 | volume %5 | vol-per-trade %6 | trades %7" )
                    .arg( market, -MARKET_STRING_WIDTH )
                    .arg( getAlpha( market ), -10 )
                    .arg( getAvgPrice( market, SIDE_BUY ), -12 )
                    .arg( getAvgPrice( market, SIDE_SELL ), -12 )
                    .arg( getVolume( market ), -12 )
                    .arg( getVolumePerTrade( market ), -12 )
                    .arg( getTrades( market ), -7 );

    }
}

QString AlphaTracker::getSaveState()
{
    QString ret;
    const QList<QString> &keys = buys.keys();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
    {
        const QString &market = *i;

        for( quint8 side = SIDE_BUY; side < SIDE_SELL +1; side++ )
        {
            QMap<QString,AlphaData> &map = ( side == SIDE_BUY ) ? buys : sells;
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

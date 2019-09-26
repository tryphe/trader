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
    AlphaData &sell_data = sells[ market ];
    AlphaData &buy_data = buys[ market ];

    return buy_data.v + sell_data.v;
}

quint64 AlphaTracker::getTrades( const QString &market )
{
    AlphaData &sell_data = sells[ market ];
    AlphaData &buy_data = buys[ market ];

    return buy_data.trades + sell_data.trades;
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
        kDebug() << QString( "%1 | alpha %2 | volume %3 | trades %4" )
                    .arg( market, -MARKET_STRING_WIDTH )
                    .arg( getAlpha( market ) )
                    .arg( getVolume( market ) )
                    .arg( getTrades( market ) );
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

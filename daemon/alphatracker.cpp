#include "alphatracker.h"
#include "position.h"
#include "global.h"

AlphaTracker::AlphaTracker()
{
}

void AlphaTracker::reset()
{
    buys.clear();
    sells.clear();
    daily_volume_epoch_secs = 0;
    daily_volume.clear();
}

void AlphaTracker::addAlpha( const QString &market, const quint8 side, const Coin &amount, const Coin &price )
{
    QMap<QString,AlphaData> &map = side == SIDE_BUY ? buys : sells;
    AlphaData &d = map[ market ];

    // for each trade, v += volume, and vp += volume * price
    d.v += amount;
    d.vp += amount * price;

    d.trades++;
}

Coin AlphaTracker::getAlpha( const QString &market ) const
{
    const Coin sell_price = getAvgPrice( market, SIDE_SELL );
    const Coin buy_price = getAvgPrice( market, SIDE_BUY );

    if ( buy_price.isZeroOrLess() || sell_price.isZeroOrLess() )
        return Coin();

    return ( sell_price / buy_price );
}

Coin AlphaTracker::getAlphaSignificanceFactor( const QString &market ) const
{
    const Coin buy_vol = getVolume( SIDE_BUY, market );
    const Coin sell_vol = getVolume( SIDE_SELL, market );

    const Coin lo = std::min( buy_vol, sell_vol );
    const Coin hi = std::max( buy_vol, sell_vol );

    return ( hi.isGreaterThanZero() ) ? lo / hi : Coin();
}

Coin AlphaTracker::getVolume( const QString &market ) const
{
    return buys.value( market ).v + sells.value( market ).v;
}

Coin AlphaTracker::getVolume( const quint8 side, const QString &market ) const
{
    return ( side == SIDE_BUY ) ? buys.value( market ).v :
           ( side == SIDE_SELL ) ? sells.value( market ).v :
                                   Coin();
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

void AlphaTracker::addDailyVolume( const qint64 epoch_time_secs, const Coin &volume )
{
    quint32 days_offset = 0;

    if ( daily_volume_epoch_secs == 0 ) // set the epoch secs to the current day at 00:00:00
        daily_volume_epoch_secs = ( QDateTime::currentSecsSinceEpoch() / 86400 ) * 86400;
    else // calculate how many days in from daily_volume_start epoch_time_secs is at
        days_offset = ( epoch_time_secs - daily_volume_epoch_secs ) / 86400;

    // add volume
    daily_volume[ days_offset ] += volume;
}

void AlphaTracker::printAlpha() const
{
    Coin total_volume, estimated_pl;
    const QList<QString> keys = getMarkets();
    for ( QList<QString>::const_iterator i = keys.begin(); i != keys.end(); i++ )
    {
        const QString &market = *i;
        const Coin volume = getVolume( market );
        const Coin significance = getAlphaSignificanceFactor( market );
        const Coin alpha = getAlpha( market );

        kDebug() << QString( "%1 | est_alpha %2 | signif %3 | buy %4 | sell %5 | vol %6 | vol-trade %7 | trades %8" )
                    .arg( market, -MARKET_STRING_WIDTH )
                    .arg( alpha.toString( 4 ), -6 )
                    .arg( significance.toString( 3 ), -5 )
                    .arg( getAvgPrice( market, SIDE_BUY ), -12 )
                    .arg( getAvgPrice( market, SIDE_SELL ), -12 )
                    .arg( volume, -12 )
                    .arg( getVolumePerTrade( market ), -12 )
                    .arg( getTrades( market ), -7 );

        // only incorporate volume and alpha into pl if we have buy and sell prices
        if ( alpha.isGreaterThanZero() )
        {
            total_volume += volume;
            // pl += (s*s)*vwap_difference*volume where s is the ratio of the total volume on each side (small_side/big_side)
            estimated_pl += significance * significance * ( ( alpha - CoinAmount::COIN ) * volume );
        }
    }

    kDebug() << "total volume:" << total_volume;
    kDebug() << "estimated pl:" << estimated_pl;
}

void AlphaTracker::printDailyVolume() const
{
    for ( QMap<quint32, Coin>::const_iterator i = daily_volume.begin(); i != daily_volume.end(); i++ )
    {
        const QDateTime iterative_date = QDateTime::fromSecsSinceEpoch( daily_volume_epoch_secs, Qt::UTC ).addDays( i.key() );

        kDebug() << QString( "%1 | %2" )
                    .arg( iterative_date.toString( "MM-dd-yyyy" ) )
                    .arg( i.value() );
    }
}

QString AlphaTracker::getSaveState() const
{
    QString ret;

    // save alpha data
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

    // save daily volume state
    QString daily_volume_data;
    for ( QMap<quint32, Coin>::const_iterator i = daily_volume.begin(); i != daily_volume.end(); i++ )
    {
        // if there's data, add a space
        if ( daily_volume_data.size() > 0 )
            daily_volume_data += QChar( ' ' );

        daily_volume_data += i.value();
    }

    // pack data prepended with header and epoch
    ret += QString( "dv %1 %2\n" )
            .arg( daily_volume_epoch_secs )
            .arg( daily_volume_data );

    return ret;
}

void AlphaTracker::readSaveState( const QString &state )
{
    QList<QString> lines = state.split( QChar( '\n' ) );

    for ( int i = 0; i < lines.size(); i++ )
    {
        const QString &line = lines.value( i );
        QList<QString> args = line.split( QChar( ' ' ) );

        // read alpha data
        if ( args.size() == 6 && args.value( 0 ) == "a" )
        {
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
        // read daily volume data
        else if ( args.size() > 2 && args.value( 0 ) == "dv" )
        {
            // read epoch secs
            daily_volume_epoch_secs = args.at( 1 ).toLongLong();
            qint64 epoch_secs_delta = daily_volume_epoch_secs;

            // read daily volume sequentially
            qint64 j;
            for ( j = 2; j < args.size(); j++ )
            {
                daily_volume.insert( j -2, args.at( j ) );
                epoch_secs_delta += 60 * 60 * 24;
            }

            // insert 0 volume for each day between epoch_secs_delta and today's date
            while ( QDateTime::fromSecsSinceEpoch( epoch_secs_delta ).daysTo( QDateTime::currentDateTime() ) > 0 )
            {
                daily_volume.insert( ++j -2, 0 );
                epoch_secs_delta += 60 * 60 * 24;
            }
        }
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

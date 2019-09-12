#include "costfunctioncache.h"
#include "coinamount.h"
#include "global.h"

#include <QDateTime>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QDataStream>
#include <QCache>

CostFunctionCache::CostFunctionCache()
{
    m_ticksize = Coin( "0.0001" );
    m_max_x = CoinAmount::COIN * 50;
}

Coin CostFunctionCache::getY( Coin profile_u, Coin reserve, Coin x )
{
    static const int FIELD_SIZE = 10;
    static const QString pre_path = Global::getCostFunctionCachePath() + QDir::separator() + "cf-";
    static const int projected_file_size = ( ( m_max_x / m_ticksize ).toInt() +1 ) * FIELD_SIZE; // +1 for zero

    /// step 1: create cache file if it doesn't exist
    // check for existing file with wrong size
    QFile f( pre_path + profile_u );
    if ( f.exists() && f.size() != projected_file_size )
    {
        kDebug() << "deleted file for wrong size" << f.size();

        // delete the file
        bool was_deleted = f.remove();
        if ( !was_deleted )
            kDebug() << "[Spruce] local error: failed to delete file" << pre_path + profile_u;

        return Coin();
    }

    // check for non-existant cache file
    if ( !f.exists() )
    {
        // open file
        if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
        {
            kDebug() << "[Spruce] local error: couldn't open savemarket file" << pre_path + profile_u;
            return Coin();
        }

        // create byte stream to write to file
        QTextStream f_out( &f );

        kDebug() << "[Spruce] generating cost function image for profile" << profile_u << "...";
        qint64 t0 = QDateTime::currentMSecsSinceEpoch();

        // y += ( 1 - y ) * profile_u;
        Coin y;
        const Coin profile = m_ticksize * 10 / profile_u;
        int bytes_written = 0;
        for ( Coin x; x <= m_max_x; x += m_ticksize /*granularity to find y*/ )
        {
            if ( !x.isZero() ) // don't skip zero, just set zero to zero
                y += ( CoinAmount::COIN - reserve - y ) * profile;

            QString data = QString( "%1" )
                    .arg( y.toAmountString(), -FIELD_SIZE );

            f_out << data;
            bytes_written += data.size();
        }
        kDebug() << "[Spruce] done generating image," << bytes_written <<
                    "bytes. took" << QDateTime::currentMSecsSinceEpoch() - t0 << "ms.";

        f_out.flush();
        f.close();
    }

    /// step 2: open the cache file and cache our value into ram
    // index of y = x / m_tick_size;
    int y_idx = ( x / m_ticksize ).toInt();

    // open file
    if ( !f.open( QIODevice::ReadOnly ) )
    {
        kDebug() << "[Spruce] local error: couldn't open savemarket file" << pre_path + profile_u;
        return Coin();
    }

    // read field
    f.seek( FIELD_SIZE * y_idx );
    QByteArray y_data = f.read( FIELD_SIZE );

    Coin y = QString::fromLocal8Bit( y_data );

    // if it's a bad value, return 0
    if ( y >= CoinAmount::COIN )
    {
        kDebug() << "[Spruce] local error: read bad y value" << y << "at index" << y_idx << "in cache file" << pre_path + profile_u;
        return Coin();
    }

    //m_cache.insert( cache_key, new Coin( y ) );

    return y;
}

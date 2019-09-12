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
    m_max_x = CoinAmount::COIN * 100;
}

Coin CostFunctionCache::getY( const Coin &profile_u, const Coin &reserve, const Coin &x )
{
    static const int MAX_RAM_CACHE = 20000; // how many 10-byte values
    static const int FIELD_SIZE = 10;
    static const QString pre_path = Global::getCostFunctionCachePath() + QDir::separator() + "cf-";
    static const int projected_file_size = ( ( m_max_x / m_ticksize ).toInt() +1 ) * FIELD_SIZE; // +1 for zero

    static_assert( MAX_RAM_CACHE > 1000 );

    /// step 1: check if it's in the cache
    QString tag = profile_u.toAmountString() + reserve.toAmountString();
    QString tag_with_x = tag + x;
    if ( m_cache.contains( tag_with_x ) )
        return m_cache.value( tag_with_x );

    /// step 2: create cache file if it doesn't exist
    // check for existing file with wrong size
    QString file_path = pre_path + tag;

    QFile f( file_path );
    if ( f.exists() && f.size() != projected_file_size )
    {
        kDebug() << "[Spruce] deleted file for wrong size" << f.size();

        // delete the file
        bool was_deleted = f.remove();
        if ( !was_deleted )
            kDebug() << "[Spruce] local error: failed to delete file" << file_path;

        return Coin();
    }

    // check for non-existant cache file
    if ( !f.exists() )
    {
        // open file
        if ( !f.open( QIODevice::WriteOnly | QIODevice::Text ) )
        {
            kDebug() << "[Spruce] local error: couldn't open savemarket file" << file_path;
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
        QString data;
        for ( Coin x; x <= m_max_x; x += m_ticksize /*granularity to find y*/ )
        {
            if ( !x.isZero() ) // don't skip zero, just set zero to zero
                y += ( CoinAmount::COIN - reserve - y ) * profile;

            data = QString( "%1" )
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
        kDebug() << "[Spruce] local error: couldn't open savemarket file" << file_path;
        return Coin();
    }

    // read field
    f.seek( FIELD_SIZE * y_idx );
    QByteArray y_data = f.read( FIELD_SIZE );

    Coin y = QString::fromLocal8Bit( y_data );

    // if it's a bad value, return 0
    if ( y >= CoinAmount::COIN )
    {
        kDebug() << "[Spruce] local error: read bad y value" << y << "at index" << y_idx << "in cache file" << file_path;
        return Coin();
    }

    // insert into cache
    m_cache.insert( tag_with_x, y );

    // check if cache is too full
    if ( m_cache.size() > MAX_RAM_CACHE + 500 )
    {
        for ( int i = 0; i < 500; i++ )
            m_cache.remove( *m_cache.keyBegin() );
    }

    return y;
}

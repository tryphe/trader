#include "qbase58.h"

#include "../libbase58/libbase58.h"

#include <QByteArray>
#include <QDebug>

static const int MAX_BASE58_LEN = 512;

QByteArray QBase58::encode( const QByteArray &in )
{
    QByteArray out;
    out.resize( MAX_BASE58_LEN );
    size_t out_size = out.size();

    const bool result = b58enc( out.data(), &out_size, in.data(), in.size() );

    if ( !result )
    {
        qDebug() << "local error: base58 encoding failed";
        return QByteArray();
    }

    // shrink the buffer to get rid of the trailing bytes and zero byte
    if ( (size_t) out.size() > out_size )
        out.resize( out_size -1 ); // -1 for zero byte

    return out;
}

QByteArray QBase58::decode( const QByteArray &in )
{
    QByteArray out;
    out.resize( MAX_BASE58_LEN );
    size_t out_size = out.size();

    const bool result = b58tobin( out.data(), &out_size, in.data(), in.size() );

    if ( !result )
    {
        qDebug() << "local error: base58 decoding failed";
        return QByteArray();
    }

    // shrink the buffer to get rid of the preceeding bytes
    if ( out_size < (size_t) out.size() )
        out.remove( 0, MAX_BASE58_LEN - out_size );

    return out;
}

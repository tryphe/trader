#include "qbase58.h"

#include "../libbase58/libbase58.h"

#include <QByteArray>
#include <QDebug>

QByteArray QBase58::encode( const QByteArray &in )
{
    QByteArray out;
    out.resize( in.size() *2 ); // note: surely there is something better than size*2, but it's better than static
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
    out.resize( in.size() *2 );
    size_t out_size = out.size();

    const bool result = b58tobin( out.data(), &out_size, in.data(), in.size() );

    if ( !result )
    {
        qDebug() << "local error: base58 decoding failed";
        return QByteArray();
    }

    // shrink the buffer to get rid of the preceeding bytes
    if ( out_size < (size_t) out.size() )
        out.remove( 0, ( in.size() *2 ) - out_size );

    return out;
}

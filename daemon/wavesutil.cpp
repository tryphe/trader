#include "wavesutil.h"
#include "../blake2b/sse/blake2.h"
#include "../qbase58/qbase58.h"

#include <vector>

#include <QByteArray>
#include <QString>
#include <QCryptographicHash>

QByteArray WavesUtil::hashBlake2b( const QByteArray &in )
{
    QByteArray blake_out;
    blake_out.resize( 32 );

    blake2b_state S[1];

    blake2b_init( S, 32 );
    blake2b_update( S, in.data(), in.length() );
    blake2b_final( S, blake_out.data(), 32 );

    return blake_out;
}

QByteArray WavesUtil::hashWaves( const QByteArray &in )
{
    const QByteArray sha3_out = QCryptographicHash::hash( hashBlake2b( in ), QCryptographicHash::Keccak_256 );
    return sha3_out;
}

void WavesUtil::clampPrivateKey( QByteArray &key )
{
    assert( key.size() > 31 );

    key[ 0  ] = key[ 0  ] & uint8_t( 248 );
    key[ 31 ] = key[ 31 ] & uint8_t( 127 );
    key[ 31 ] = key[ 31 ] | uint8_t( 64 );
}

QByteArray WavesUtil::getAssetBytes( const QString &asset )
{
    QByteArray ret;

    if ( asset == QLatin1String( "WAVES" ) )
    {
        ret += '\x00';
        assert( ret.size() == 1 );
    }
    else
    {
        ret += '\x01';
        ret += QBase58::decode( asset.toLocal8Bit() );
        assert( ret.size() == 33 );
    }

    return ret;
}

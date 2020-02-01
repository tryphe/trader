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

void WavesUtil::test()
{
    /// test private key clamp on zeroed out key
    QByteArray pkey;
    pkey.resize( 32 );
    pkey.fill( 0x00 );

    clampPrivateKey( pkey );
    assert( QBase58::encode( pkey ) == "111111111111111111111111111111127" );

    /// test blake2b hash
    assert( hashBlake2b( QByteArray() ).toHex() == "0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8" );
    assert( hashBlake2b( QByteArray( "Hello World!\n") ).toHex() == "f497a36252fe0182836a19a75de5d75996a0f0a4e19f81fe749aa9e809a1150c" );

    /// test waves hash
    assert( hashWaves( QByteArray( "A nice, long test to make the day great! :-)" ) ).toHex() == "5df3cf20205d75e09ae46d13a8d99a16174d71c84ffcc00387fec3d81e39dcbe" );

    /// test binary serialization of assets
    assert( WavesUtil::getAssetBytes( "WAVES" ) == QByteArray::fromHex( "00" ) );
    assert( WavesUtil::getAssetBytes( "8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS" ) == QByteArray::fromHex( "01" ) + QBase58::decode( "8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS" ) );
}

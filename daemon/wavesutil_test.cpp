#include "wavesutil_test.h"
#include "wavesutil.h"
#include "../qbase58/qbase58.h"

#include <QByteArray>
#include <QString>

void WavesUtilTest::test()
{
    /// test private key clamp on zeroed out key
    QByteArray pkey;
    pkey.resize( 32 );
    pkey.fill( 0x00 );

    WavesUtil::clampPrivateKey( pkey );
    assert( QBase58::encode( pkey ) == "111111111111111111111111111111127" );

    /// test blake2b hash
    assert( WavesUtil::hashBlake2b( QByteArray() ).toHex() == "0e5751c026e543b2e8ab2eb06099daa1d1e5df47778f7787faab45cdf12fe3a8" );
    assert( WavesUtil::hashBlake2b( QByteArray( "Hello World!\n") ).toHex() == "f497a36252fe0182836a19a75de5d75996a0f0a4e19f81fe749aa9e809a1150c" );

    /// test waves hash
    assert( WavesUtil::hashWaves( QByteArray( "A nice, long test to make the day great! :-)" ) ).toHex() == "5df3cf20205d75e09ae46d13a8d99a16174d71c84ffcc00387fec3d81e39dcbe" );

    /// test binary serialization of assets
    assert( WavesUtil::getAssetBytes( "WAVES" ) == QByteArray::fromHex( "00" ) );
    assert( WavesUtil::getAssetBytes( "8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS" ) == QByteArray::fromHex( "01" ) + QBase58::decode( "8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS" ) );
}

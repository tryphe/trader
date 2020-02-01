#include "qbase58_test.h"
#include "qbase58.h"

#include <QByteArray>

void QBase58Test::test()
{
    const QByteArray encoded = "CMLwxbMZJMztyTJ6Zkos66cgU7DybfFJfyJtTVpme54t";
    const QByteArray decoded = QByteArray::fromHex( "a8a6ba2678d1983ad78cfd1aff049367a91c4274b46674ea51d99fc4a3f3c159" );

    assert( QBase58::decode( encoded ) == decoded );
    assert( QBase58::encode( decoded ) == encoded );
}

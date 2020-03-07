#include "wavesaccount_test.h"
#include "wavesaccount.h"
#include "position.h"

#include <QByteArray>

void WavesAccountTest::test()
{
    /// test private_key -> public_key -> address
    const QByteArray private_key_b58 = "CMLwxbMZJMztyTJ6Zkos66cgU7DybfFJfyJtTVpme54t";
    const QByteArray public_key_b58 = "8LbAU5BSrGkpk5wbjLMNjrbc9VzN9KBBYv9X8wGpmAJT";
    const QByteArray testnet_addr_b58 = "3MzZCGFyuxgC4ZmtKRS7vpJTs75ZXdkbp1K";

    WavesAccount acc;
    acc.setPrivateKeyB58( private_key_b58 );

    assert( acc.privateKeyB58() == private_key_b58 );
    assert( acc.publicKeyB58() == public_key_b58 );
    assert( acc.address( WavesAccount::TESTNET ) == testnet_addr_b58 );

    acc.setPrivateKey( QByteArray::fromHex( "88727a03377bfba1b3655c5ecb978da171e024aad722ee49fff9214a747e7061" ) );

    /// test sign and verify
    const QByteArray message = QByteArray::fromHex( "0102030405" );

    /// generate signature
    QByteArray signature;
    bool sign_result = acc.sign( message, signature );

    assert( sign_result );
    assert( signature.size() > 0 );

    /// verify our message signed with a random signature
    bool verify_result = acc.verify( message, signature );

    assert( verify_result );

    /// verify our message signed with a non-random signature
    sign_result = acc.sign( message, signature, false );

    assert( sign_result );
    assert( QBase58::encode( signature ) == "5HRwSL8XGhSEbtLuBfZT1AkfKaybYA67aKTUpg3v47aTZfCLiBMDLj1P9PmoirWcNCVFCoja4gmv5nkjDnYAULus" );

    /// verify that it's false with the wrong message but the correct signature
    QByteArray wrong_message = message + "1";
    verify_result = acc.verify( wrong_message, signature );

    assert( !verify_result );

    /// verify that it's false with the correct message but the wrong signature
    QByteArray wrong_signature = signature;
    wrong_signature[ 0 ] = 0x00;
    verify_result = acc.verify( message, wrong_signature );

    assert( !verify_result );

    /// test cancel order bytes without random signature bytes
    QByteArray order_id_b58 = "H93RaJ6D9YxEWNJiiMsej23NVHLrxu6kMyFb7CgX2DZW";
    acc.setPrivateKeyB58( "CrppxhgtZZNd5wcVMwsudWJ78ZKLqETR8AmhtjeKDFZU" );

    QByteArray cancel_order_bytes = acc.createCancelBytes( order_id_b58 );

    sign_result = acc.sign( cancel_order_bytes, signature, false );

    assert( QBase58::encode( cancel_order_bytes ) == "4W1eSfcBttw6kiyZhhe52DamKjdcQgqGapg1VFVp5pNqgyomPVMi6NRAr6cLiCi1dAQoUni7eQETMBsjMu1fNLbn" );

    /// test cancel order body without random signature bytes
    order_id_b58 = "6j1ccV4SV2FtNUUUKGwYJVEVvH5L8iXKiawrmoWCtqV2";
    acc.setPrivateKeyB58( "7V7gbGjXBiLGJf8aUNUP4Vzm5kuMNHd3Gt5THRTmYMfd" );

    assert( acc.createCancelBody( order_id_b58, false ) == "{\"orderId\":\"6j1ccV4SV2FtNUUUKGwYJVEVvH5L8iXKiawrmoWCtqV2\",\"proofs\":[\"3AP6y9ye9eaDzFP7LaAdxkDYf6YXFBHVFJ9Dqy4PLBQPNdvwyJMGUFs3xLx1JFxNext8xhonPKhdyZQ6GLYCdZhD\"],\"sender\":\"27YM9icwd6TwfZD3KEJpYsj7rLwPAShJdYXrCt8QRo6L\",\"senderPublicKey\":\"27YM9icwd6TwfZD3KEJpYsj7rLwPAShJdYXrCt8QRo6L\",\"signature\":\"3AP6y9ye9eaDzFP7LaAdxkDYf6YXFBHVFJ9Dqy4PLBQPNdvwyJMGUFs3xLx1JFxNext8xhonPKhdyZQ6GLYCdZhD\"}" );

    /// test creating an order id v2 with the above keys
    acc.initAssetMaps(); // init asset aliases
    acc.setMatcherPublicKeyB58( "9cpfKN9suPNvfeUNphzxXMjcnn974eme8ZhWUjaktzU5" );

    Position pos = Position( "BTC_WAVES", SIDE_SELL, "", "0.01000000", "0.00097" );

    QByteArray order_bytes_v2 = acc.createOrderBytes( &pos, CoinAmount::SATOSHI, CoinAmount::SATOSHI, quint64( 1580472938469 ), quint64( 1582978538468 ) );

    assert( acc.createOrderId( order_bytes_v2 ) == "DH2Uyfdoj2pj1t1EEbLPYJMVRcWYqw6kBgQkVZjNiE2o" );

    /// test creating order body from the order byes above
    assert( acc.createOrderBody( &pos, CoinAmount::SATOSHI, CoinAmount::SATOSHI, quint64( 1580472938469 ), quint64( 1582978538468 ) , false ) == "{\"amount\":9700000,\"assetPair\":{\"amountAsset\":\"WAVES\",\"priceAsset\":\"8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS\"},\"expiration\":1582978538468,\"id\":\"DH2Uyfdoj2pj1t1EEbLPYJMVRcWYqw6kBgQkVZjNiE2o\",\"matcherFee\":300000,\"matcherPublicKey\":\"9cpfKN9suPNvfeUNphzxXMjcnn974eme8ZhWUjaktzU5\",\"orderType\":\"sell\",\"price\":1000000,\"proofs\":[\"DCKsiyJu1avWRDe3Zr5Wxt2T1A352T1TosxwUiaQEaTDqYoNC7D9N3fa6fDjGLL3QRbxKnovKchrMCJb6fv1d5y\"],\"senderPublicKey\":\"27YM9icwd6TwfZD3KEJpYsj7rLwPAShJdYXrCt8QRo6L\",\"timestamp\":1580472938469,\"version\":2}" );

    /// test creating get orders bytes
    QByteArray get_orders_bytes = acc.createGetOrdersBytes( qint64( 0 ) );

    assert( get_orders_bytes == QByteArray::fromHex( "10889dcdbba87fac60ac658f4c8b4add9e7aafecc3fff1fafb599d833b38307d0000000000000000" ) );
}

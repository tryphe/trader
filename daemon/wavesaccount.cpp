#include "wavesaccount.h"
#include "wavesutil.h"
#include "position.h"

#include "../qbase58/qbase58.h"
#include "../libcurve25519-donna/additions/keygen.h"
#include "../libcurve25519-donna/additions/curve_sigs.h"

#include <QDebug>
#include <QRandomGenerator>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDataStream>

WavesAccount::WavesAccount()
{
}

void WavesAccount::setPrivateKey( const QByteArray &new_private_key )
{
    if ( new_private_key.size() < 32 )
    {
        kDebug() << "local error: WavesAccount::setPrivateKey: tried to set new private key with size <32";
        return;
    }

    // set privkey
    private_key = new_private_key;

    WavesUtil::clampPrivateKey( private_key );

    // clear and set pubkey with privkey
    public_key.clear();
    public_key.resize( 32 );

    curve25519_keygen( reinterpret_cast<uint8_t*>( public_key.data() ),
                       reinterpret_cast<uint8_t*>( private_key.data() ) );
}

void WavesAccount::setPrivateKeyB58( const QByteArray &new_private_key_b58 )
{
    setPrivateKey( QBase58::decode( new_private_key_b58 ) );
}

QByteArray WavesAccount::address( const uint8_t network ) const
{
    if ( public_key.size() < 32 )
    {
        kDebug() << "local error: WavesAccount::address: tried to get address from empty public key";
        return QByteArray();
    }

    QByteArray pubkey_hash = WavesUtil::hashWaves( public_key );
    QByteArray addr;

    // add entity_type_byte + network_byte + pubkey_hash.left(20)
    addr.append( uint8_t( 1 ) );
    addr.append( network );
    addr.append( pubkey_hash.left( 20 ) );

    // reuse pubkey_hash to compute the checksum
    pubkey_hash = WavesUtil::hashWaves( addr );
    addr.append( pubkey_hash.left( 4 ) );

    assert( addr.size() == 26 );

    return QBase58::encode( addr );
}

bool WavesAccount::sign( const QByteArray &message, QByteArray &signature, bool add_random_bytes ) const
{
    if ( private_key.size() < 32 )
    {
        kDebug() << "local error: WavesAccount::sign: private key size <32";
        return false;
    }

    // generate random bytes
    QByteArray random_bytes;
    random_bytes.resize( 64 );
    random_bytes.fill( 0x00 );

    if ( add_random_bytes )
        QRandomGenerator::global()->generate( random_bytes.begin(), random_bytes.end() );

    // assure correct signature buffer size
    signature.resize( 64 );

    const int ret = curve25519_sign( reinterpret_cast<uint8_t*>( signature.data() ),
                                     reinterpret_cast<const uint8_t*>( private_key.constData() ),
                                     reinterpret_cast<const uint8_t*>( message.constData() ),
                                     message.size(),
                                     reinterpret_cast<const uint8_t*>(random_bytes.constData() ) );

    return ret == 0;
}

bool WavesAccount::verify( const QByteArray &message, const QByteArray &signature ) const
{
    if ( public_key.size() < 32 )
    {
        kDebug() << "local error: WavesAccount::verify: public key size <32";
        return false;
    }

    const int ret = curve25519_verify( reinterpret_cast<const uint8_t*>( signature.data() ),
                                       reinterpret_cast<const uint8_t*>( public_key.constData() ),
                                       reinterpret_cast<const uint8_t*>( message.constData() ),
                                       message.size() );
    return ret == 0;
}

void WavesAccount::initAssetMaps()
{
    const QString ALIAS_USD =   "Ft8X1v1LTa1ABafufpaCWyVj8KkaxUWE6xBhW6sNFJck",
                  ALIAS_USDN =  "DG2xFkPdDwKUoBkzGAhQtLpSGzfXLiCYPEzeKH2Ad24p",
                  ALIAS_EUR =   "Gtb1WRznfchDnTh37ezoDTJ4wcoKaRsKqKjJjy7nm2zU",
                  ALIAS_TRY =   "2mX5DzVKWrAJw8iwdJnV2qtoeVG9h5nTDpTqC1wb1WEN",
                  ALIAS_BTC =   "8LQW8f7P5d5PZM7GtZEBgaqRPGSzS3DfPuiXrURJ4AJS",
                  ALIAS_WAVES = "WAVES",
                  ALIAS_ETH =   "474jTeYx2r2Va35794tCScAXWJG9hU2HcgxzMowaZUnu",
                  ALIAS_BCH =   "zMFqXuoyrn5w17PFurTqxB7GsS71fp9dfk6XFwxbPCy",
                  ALIAS_BSV =   "62LyMjcr2DtiyF5yVXFhoQ2q414VPPJXjsNYp72SuDCH",
                  ALIAS_LTC =   "HZk1mbfuJpmxU1Fs4AX5MWLVYtctsNcg6e2C6VKqK8zk",
                  ALIAS_DASH =  "B3uGHFRpSUuGEDWjqB9LWWxafQj8VTvpMucEyoxzws5H",
                  ALIAS_XMR =   "5WvPKSJXzVE2orvbkJ8wsQmmQKqTv9sGBPksV4adViw3",
                  ALIAS_ZEC =   "BrjUWjndUanm5VsJkbUip8VRYy6LWJePtxya3FNv4TQa";

    asset_by_alias.insert( ALIAS_USD, "USD" );
    asset_by_alias.insert( ALIAS_USDN, "USDN" );
    asset_by_alias.insert( ALIAS_EUR, "EUR" );
    asset_by_alias.insert( ALIAS_TRY, "TRY" );
    asset_by_alias.insert( ALIAS_BTC, "BTC" );
    asset_by_alias.insert( ALIAS_WAVES, "WAVES" );
    asset_by_alias.insert( ALIAS_ETH, "ETH" );
    asset_by_alias.insert( ALIAS_BCH, "BCH" );
    asset_by_alias.insert( ALIAS_BSV, "BSV" );
    asset_by_alias.insert( ALIAS_LTC, "LTC" );
    asset_by_alias.insert( ALIAS_DASH, "DASH" );
    asset_by_alias.insert( ALIAS_XMR, "XMR" );
    asset_by_alias.insert( ALIAS_ZEC, "ZEC" );

    // add all of the aliases above into price_assets (they are all price assets)
    for ( QMap<QString,QString>::const_iterator i = asset_by_alias.begin(); i != asset_by_alias.end(); i++ )
        price_assets += i.key();

    // duplicate the above map into reverse access map alias_by_asset
    for ( QMap<QString,QString>::const_iterator i = asset_by_alias.begin(); i != asset_by_alias.end(); i++ )
        alias_by_asset.insert( i.value(), i.key() );
}

QByteArray WavesAccount::createCancelBytes( const QByteArray &order_id_b58 ) const
{
    QByteArray cancel_order_bytes;
    cancel_order_bytes += publicKey();
    cancel_order_bytes += QBase58::decode( order_id_b58 );

    assert( cancel_order_bytes.size() == 64 );

    return cancel_order_bytes;
}

QByteArray WavesAccount::createCancelBody( const QByteArray &order_id_b58, bool random_sign_bytes ) const
{
    if ( public_key.size() < 32 )
    {
        kDebug() << "local error: WavesAccount::createCancelBody: account public key is empty";
        return QByteArray();
    }

    const QByteArray cancel_order_bytes = createCancelBytes( order_id_b58 );
    QByteArray signature;
    const bool sign_result = sign( cancel_order_bytes, signature, random_sign_bytes );

    assert( sign_result );

    //qDebug() << acc.publicKeyB58();
    //qDebug() << cancel_order_bytes.size() << cancel_order_bytes.toHex();

    QJsonObject obj;
    obj[ "orderId" ] = QString( order_id_b58 );
    obj[ "sender" ] = QString( publicKeyB58() );
    obj[ "senderPublicKey" ] = QString( publicKeyB58() );
    obj[ "signature" ] = QString( QBase58::encode( signature ) );
    obj[ "proofs" ] = QJsonArray{ QString( QBase58::encode( signature ) ) };

    // jsonify object
    QJsonDocument doc;
    doc.setObject( obj );

    return doc.toJson( QJsonDocument::Compact );
}

QByteArray WavesAccount::createOrderBytes( Position * const &pos, const qint64 epoch_now, const qint64 epoch_expiration ) const
{
    if ( matcher_public_key.size() < 32 ||
         public_key.size() < 32 ||
         alias_by_asset.size() == 0 )
    {
        kDebug() << "local error: WavesAccount::createOrderBytes: account pubkey or matcher pubkey is empty, or asset aliases are empty";
        return QByteArray();
    }

    QByteArray order_id_v2;
    order_id_v2 += 0x02; // version byte
    order_id_v2 += publicKey();
    order_id_v2 += matcher_public_key;
    order_id_v2 += WavesUtil::getAssetBytes( alias_by_asset.value( pos->market.getQuote() ) );
    order_id_v2 += WavesUtil::getAssetBytes( alias_by_asset.value( pos->market.getBase() ) );
    order_id_v2 += pos->side == SIDE_SELL ? WavesUtil::SELL : WavesUtil::BUY;

    QDataStream order_id_v2_stream( &order_id_v2, QIODevice::WriteOnly );
    order_id_v2_stream.device()->seek( order_id_v2.size() );

    order_id_v2_stream << pos->price.toIntSatoshis(); // price = 1000000
    order_id_v2_stream << pos->quantity.toIntSatoshis(); // amount = 9700000
    order_id_v2_stream << epoch_now; // order set time +1 minute
    order_id_v2_stream << epoch_expiration; // expiration time
    order_id_v2_stream << 300000LL; // matcher fee = 300000

    return order_id_v2;
}

QByteArray WavesAccount::createOrderId( const QByteArray &order_bytes ) const
{
    return QBase58::encode( WavesUtil::hashBlake2b( order_bytes ) );
}

QByteArray WavesAccount::createOrderBody( Position * const &pos, const qint64 epoch_now, const qint64 epoch_expiration, bool random_sign_bytes ) const
{
    if ( matcher_public_key.size() < 32 ||
         public_key.size() < 32 ||
         alias_by_asset.size() == 0 )
    {
        kDebug() << "local error: WavesAccount::createOrderBody: account pubkey or matcher pubkey is empty, or asset aliases are empty";
        return QByteArray();
    }

    // note: 29d expiration == + ( 60000LL * 60LL * 24LL * 29LL )
    const qint64 current_time = QDateTime::currentMSecsSinceEpoch();
    const QByteArray order_bytes_v2 = createOrderBytes( pos, epoch_now, epoch_expiration );
    const QByteArray order_id_v2_b58 = createOrderId( order_bytes_v2 );
    QByteArray signature;

    const bool sign_result = sign( order_bytes_v2, signature, random_sign_bytes );

    assert( sign_result );

    QJsonObject order_body_v2;
    // put transaction bytes
    order_body_v2[ "orderType" ] = pos->side == SIDE_BUY ? "buy" : "sell";
    order_body_v2[ "version" ] = 2;
    order_body_v2[ "assetPair" ] = QJsonObject{ { "amountAsset", alias_by_asset.value( pos->market.getQuote() ) },
                                                { "priceAsset", alias_by_asset.value( pos->market.getBase() ) } };
    order_body_v2[ "price" ] = pos->price.toIntSatoshis();
    order_body_v2[ "amount" ] = pos->quantity.toIntSatoshis();
    order_body_v2[ "timestamp" ] = epoch_now;
    order_body_v2[ "expiration" ] = epoch_expiration;
    order_body_v2[ "matcherFee" ] = 300000;
    order_body_v2[ "matcherPublicKey" ] = QString( QBase58::encode( matcher_public_key ) );
    order_body_v2[ "senderPublicKey" ] = QString( QBase58::encode( public_key ) );

    // put order id and signature
    order_body_v2[ "id" ] = QString( order_id_v2_b58 );
    order_body_v2[ "proofs" ] = QJsonArray{ QString( QBase58::encode( signature ) ) };

    // jsonify object
    QJsonDocument doc;
    doc.setObject( order_body_v2 );

    return doc.toJson( QJsonDocument::Compact );
}

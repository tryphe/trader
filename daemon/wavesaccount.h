#ifndef WAVESACCOUNT_H
#define WAVESACCOUNT_H

#include "../qbase58/qbase58.h"

#include <QByteArray>
#include <QMap>
#include <QStringList>

class Position;
class WavesREST;

class WavesAccount
{
public:
    static const uint8_t MAINNET = 'W';
    static const uint8_t TESTNET = 'T';

    WavesAccount();

    /// crypto stuff
    void setPrivateKey( const QByteArray &new_private_key );
    void setPrivateKeyB58( const QByteArray &new_private_key_b58 );
    void setMatcherPublicKeyB58( const QByteArray &new_matcher_public_key_b58 ) { matcher_public_key = QBase58::decode( new_matcher_public_key_b58 ); }

    QByteArray privateKey() const { return private_key; }
    QByteArray privateKeyB58() const { return QBase58::encode( private_key ); }
    QByteArray publicKey() const { return public_key; }
    QByteArray publicKeyB58() const { return QBase58::encode( public_key ); }
    QByteArray address( const uint8_t network = MAINNET ) const;

    bool sign( const QByteArray &message, QByteArray &signature, bool add_random_bytes = true ) const;
    bool verify( const QByteArray &message, const QByteArray &signature ) const;

    /// order stuff
    void initAssetMaps();

    QString getAliasByAsset( const QString &asset ) const { return alias_by_asset.value( asset ); }
    QString getAssetByAlias( const QString &asset ) const { return asset_by_alias.value( asset ); }
    const QStringList &getPriceAssets() const { return price_assets; }

    QByteArray createCancelBytes( const QByteArray &order_id_b58 ) const;
    QByteArray createCancelBody( const QByteArray &order_id_b58, bool random_sign_bytes = true ) const;

    QByteArray createOrderBytes( Position *const &pos, const qint64 epoch_now, const qint64 epoch_expiration ) const;
    QByteArray createOrderId( const QByteArray &order_bytes ) const;
    QByteArray createOrderBody( Position *const &pos, const qint64 epoch_now, const qint64 epoch_expiration, bool random_sign_bytes = true ) const;

    QByteArray createGetOrdersBytes( const qint64 epoch_now );

private:
    QByteArray private_key, public_key, matcher_public_key;

    // asset mappings
    QMap<QString,QString> asset_by_alias, alias_by_asset;
    QStringList price_assets;
};

#endif // WAVESACCOUNT_H

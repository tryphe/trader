#ifndef WAVESUTIL_H
#define WAVESUTIL_H

#include <QByteArray>

namespace WavesUtil
{
    const uint8_t BUY = 0;
    const uint8_t SELL = 1;

    QByteArray hashBlake2b( const QByteArray &in );
    QByteArray hashWaves( const QByteArray &in );

    void clampPrivateKey( QByteArray &key );

    QByteArray getAssetBytes( const QString &asset );

    void test();
}

#endif // WAVESUTIL_H

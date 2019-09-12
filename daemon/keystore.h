#ifndef KEYSTORE_H
#define KEYSTORE_H

#include <assert.h>

#include <QByteArray>
#include <QPair>
#include <QVector>
#include <QRandomGenerator>


class KeyStore
{
public:
    static const int pad_size = 32; // pad_size must be divisible by 4
    static const int junk_factor = 12; // set greater than 1
    static const int sane_minimum_size = 32*2;
    static const int pad_size_final = std::max( sane_minimum_size, std::min( pad_size * ( junk_factor ), std::numeric_limits<qint16>::max() - ( pad_size * junk_factor ) ) );

    static_assert( pad_size % 4 == 0 );
    static_assert( pad_size > 0 );
    static_assert( junk_factor > 1 );

    explicit KeyStore() { test(); }
    void setKeys( const QByteArray &key, const QByteArray &secret )
    {
        // setKeys() essentially refreshes our store
        clear();

        // generate pad
        for ( int i = 0; i < pad_size_final; i += 4 )
        {
            quint32 rand = QRandomGenerator::system()->generate();
            m_pad += quint8( rand >> 24 );
            m_pad += quint8( rand >> 16 );
            m_pad += quint8( rand >> 8 );
            m_pad += quint8( rand );
        }
        //qDebug() << m_pad.toHex();
        assert( m_pad.size() == pad_size_final );

        // generate offset to read from
        m_offset = QRandomGenerator::system()->generate() % ( pad_size * junk_factor );

        const QByteArray &pad_to_use = m_getPad();
        m_key = xorDecodeEncode( key, pad_to_use );
        m_secret = xorDecodeEncode( secret, pad_to_use );

        //qDebug() << "pad size:" << m_pad.size() << "offset:" << m_offset;
        //qDebug() << "key:" << m_key.toHex() << "secret:" << m_secret.toHex();
    }

    void test()
    {
        const QByteArray test_key = QByteArray::fromHex("07D1");
        const QByteArray test_secret = QByteArray::fromHex("F877");

        setKeys( test_key, test_secret );

        assert( getKey() == test_key );
        assert( getSecret() == test_secret );
    }

    void clear()
    {
        m_pad.clear();
        m_key.clear();
        m_secret.clear();
        m_offset = 0;
    }

    static QByteArray xorDecodeEncode( const QByteArray &m /*msg*/, const QByteArray &k /*key*/ )
    {
        if ( k.isEmpty() || m.isEmpty() )
            return QByteArray();

        QByteArray ret;
        for ( int i = 0; i < m.size(); i++ )
            ret += m[ i ] ^ k.at( i % k.size() );

        assert( ret.size() == m.size() );
        return ret;
    }

    bool isKeyOrSecretEmpty() const { return m_key.isEmpty() || m_secret.isEmpty(); }
    QByteArray getKey() { return xorDecodeEncode( m_key, m_getPad() ); }
    QByteArray getSecret() { return xorDecodeEncode( m_secret, m_getPad() ); }
    QPair<QByteArray, QByteArray> getKeyAndSecret() { const QByteArray &pad_to_use = m_getPad();
                                                      return QPair<QByteArray, QByteArray>( xorDecodeEncode( m_key, pad_to_use ),
                                                                                            xorDecodeEncode( m_secret, pad_to_use ) ); }
private:
    const QByteArray m_getPad() const { return m_pad.mid( m_offset, pad_size ); }

    QByteArray m_key, m_secret, m_pad;
    quint16 m_offset;
};


#endif // KEYSTORE_H

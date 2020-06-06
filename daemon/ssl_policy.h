#ifndef SSLPOLICY_H
#define SSLPOLICY_H

#include <assert.h>
#include "global.h"

#include <QSslConfiguration>
#include <QSslCipher>
#include <QDebug>
#include <QLoggingCategory>


namespace SslPolicy
{

static inline void enableSecureSsl()
{
    // disable chatty messages for disabled ssl stuff (might suppress problems with ssl in the future)
    //QLoggingCategory::setFilterRules( "qt.network.ssl.warning=false" );

    // get default config
    QSslConfiguration ssl_config = QSslConfiguration::defaultConfiguration();

    // enforce tls minimum version
    ssl_config.setProtocol( QSsl::TlsV1_2OrLater );

    // enable ocsp (don't do this, it breaks things)
    //ssl_config.setOcspStaplingEnabled( true );

    // disable legacy reneg
    ssl_config.setSslOption( QSsl::SslOptionDisableLegacyRenegotiation, true );

    // disable compression
    ssl_config.setSslOption( QSsl::SslOptionDisableCompression, true );

    QList<QSslCipher> cipher_list = ssl_config.supportedCiphers();
    QList<QSslCipher> chosen_ciphers;

    QSet<QString> bad_ciphers;
    // old/weak
    bad_ciphers += "DHE-DSS-AES256-SHA256";
    bad_ciphers += "DHE-DSS-AES128-SHA256";
    bad_ciphers += "AES128-GCM-SHA256";
    bad_ciphers += "AES256-GCM-SHA384";
    bad_ciphers += "AES256-SHA256";
    bad_ciphers += "AES128-SHA256";
    bad_ciphers += "RSA-PSK-AES128-GCM-SHA256";
    bad_ciphers += "RSA-PSK-AES256-GCM-SHA384";
    bad_ciphers += "RSA-PSK-CHACHA20-POLY1305";
    bad_ciphers += "DHE-RSA-AES128-SHA256";
    bad_ciphers += "DHE-RSA-AES256-SHA256";
    bad_ciphers += "ECDH-RSA-AES128-SHA256";
    bad_ciphers += "ECDH-RSA-AES256-SHA384";
    bad_ciphers += "ECDH-ECDSA-AES256-SHA384";
    bad_ciphers += "ECDH-ECDSA-AES128-SHA256";
    bad_ciphers += "ECDHE-RSA-AES128-SHA256";
    bad_ciphers += "ECDHE-RSA-AES256-SHA256";
    bad_ciphers += "ECDHE-ECDSA-AES128-SHA256";
    bad_ciphers += "ECDHE-ECDSA-AES256-SHA384";
    bad_ciphers += "ECDHE-RSA-AES256-SHA384";

    // no forward secrecy
    bad_ciphers += "DHE-RSA-AES128-GCM-SHA256";
    bad_ciphers += "DHE-PSK-AES128-GCM-SHA256";
    bad_ciphers += "DHE-PSK-AES256-GCM-SHA384";
    bad_ciphers += "DHE-PSK-CHACHA20-POLY1305";
    bad_ciphers += "ECDHE-PSK-CHACHA20-POLY1305";
    bad_ciphers += "PSK-AES128-GCM-SHA256";
    bad_ciphers += "PSK-AES256-GCM-SHA384";
    bad_ciphers += "PSK-CHACHA20-POLY1305";

    // choose ciphers
    for ( int i = 0; i < cipher_list.size(); i++ )
    {
        const QSslCipher &cipher = cipher_list.at( i );
        const QString &cipher_str = cipher.name();
        const QString &proto_str = cipher.protocolString();

        // remove shit ciphers and protocols
        if ( proto_str == "TLSv1" || // for some reason setProtocol does not remove v1 proto ciphers
             cipher_str.contains( "MD5-" ) || // any md5
             cipher_str.contains( "RC4-" ) || // any rc4
             cipher_str.contains( "DES-" ) || // any des/3des
             cipher_str.endsWith( "-SHA" ) || // any counter mode with sha1
             bad_ciphers.contains( cipher_str ) )
        {
#if defined(PRINT_DISABLED_SSL_CIPHERS)
            kDebug() << "[SSL] disabled" << proto_str << cipher_str;
#endif
            continue;
        }

#if defined(PRINT_ENABLED_SSL_CIPHERS)
        kDebug() << "[SSL] enabled" << proto_str << cipher_str;
#endif

        chosen_ciphers.append( cipher );
    }

    // check for ciphers
    if ( chosen_ciphers.isEmpty() )
    {
        kDebug() << "[SSL] local error: no secure ciphers found";
        exit( 11 );
    }

    // set ciphers as default config for this application
    ssl_config.setCiphers( chosen_ciphers );
    QSslConfiguration::setDefaultConfiguration( ssl_config );
    assert( QSslConfiguration::defaultConfiguration().ciphers() == chosen_ciphers );
}

}

#endif // SSLPOLICY_H

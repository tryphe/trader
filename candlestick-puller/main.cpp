#include "puller.h"

#include "../daemon/ssl_policy.h"

#include <QCoreApplication>

int main(int argc, char *argv[])
{
    QCoreApplication a( argc, argv );
    SslPolicy::enableSecureSsl();
    Puller *p = new Puller(); Q_UNUSED( p );

    return a.exec();
}

#ifndef WAVESREST_H
#define WAVESREST_H

#include <QObject>

#include "global.h"
#include "position.h"
#include "keystore.h"
#include "baserest.h"

class QNetworkReply;
class QTimer;

class WavesREST : public BaseREST
{
    Q_OBJECT

    friend class CommandRunner;

public:
    explicit WavesREST( Engine *_engine, QNetworkAccessManager *_nam );
    ~WavesREST();

    void init();

public Q_SLOTS:
    void sendNamQueue();
    void onNamReply( QNetworkReply *const &reply );

    void onCheckBotOrders();
    void onCheckTicker();

private:
    QTimer *send_timer{ nullptr };
    QTimer *orderbook_timer{ nullptr };
    QTimer *ticker_timer{ nullptr };
};

#endif // WAVESREST_H

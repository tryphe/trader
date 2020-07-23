#ifndef PULLER_H
#define PULLER_H

#include "../daemon/coinamount.h"
#include "../daemon/priceaggregator.h"

#include <QMap>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QNetworkRequest>

class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;
class TickerRequest;

struct CandleRequest
{
    QNetworkRequest request;
    QString id;
};

class Puller : public QObject
{
    Q_OBJECT

public:
    explicit Puller();
    ~Puller();

    void sendCandleRequest();

    void sendRequest( QString url, QString query, QString id );

public Q_SLOTS:
    void onSendNameQueue();
    void onCheckFinished();
    void onNamReply( QNetworkReply *reply );

private:
    QNetworkAccessManager *nam;

    QMap<QNetworkReply*, QString> nam_queue_sent;
    QVector<CandleRequest*> nam_queue;

    bool waiting_for_final_reply{ false }; // tracks if we are done
    bool invert_price{ false };
    bool continue_on_empty_data{ false };
    bool updating_candles{ false };

    qint64 last_sample_secs{ 0 };
    qint64 requests_made{ 0 };
    qint64 requests_parsed{ 0 };

    PriceData price_data;
    QDateTime current_date;
    QString current_market;
    QString filename;
};

#endif // PULLER_H

#ifndef BASEREST_H
#define BASEREST_H

#include "global.h"
#include "misctypes.h"
#include "keystore.h"

#include <QObject>
#include <QQueue>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QThread>

class QNetworkAccessManager;
class QNetworkReply;
class Request;
class Stats;
class Engine;
class QTimer;
class Position;

struct BaseREST : public QObject
{
    explicit BaseREST( Engine *_engine );
    ~BaseREST();

    void sendRequest( QString api_command, QString body = QLatin1String(), Position *pos = nullptr, quint16 weight = 0 );

    bool isKeyOrSecretUnset() const;
    bool isCommandQueued( const QString &api_command ) const;
    bool isCommandSent( const QString &api_command, qint32 min_times = 1 ) const;
    void removeRequest( const QString &api_command, const QString &body );
    void deleteReply( QNetworkReply *const &reply, Request *const &request );

    QQueue<Request*> nam_queue; // queue for requests so we can load balance timestamp/hmac generation
    QHash<QNetworkReply*,Request*> nam_queue_sent; // request tracking queue

    KeyStore keystore;

    qint64 request_nonce;
    qint64 last_request_sent_ms;

    qint64 orderbook_update_time; // set this to most recent trade
    qint64 orderbook_update_request_time;
    qint64 orderbook_public_update_time;
    qint64 orderbook_public_update_request_time;
    qint32 limit_commands_queued;
    qint32 limit_commands_queued_dc_check;
    qint32 limit_commands_sent;
    qint32 limit_timeout_yield;
    qint32 market_cancel_thresh;

    qint64 slippage_stale_time;
    qint64 orderbook_stale_tolerance, orders_stale_trip_count, books_stale_trip_count;

    QTimer *send_timer;
    QTimer *timeout_timer;
    QTimer *orderbook_timer;
    QTimer *diverge_converge_timer;
    QTimer *ticker_timer;

    AvgResponseTime avg_response_time;

    QNetworkAccessManager *nam;
    Stats *stats;
    Engine *engine;
};


#endif // BASEREST_H

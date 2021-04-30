#ifndef BASEREST_H
#define BASEREST_H

#include "global.h"
#include "misctypes.h"
#include "keystore.h"

#include <QObject>
#include <QQueue>
#include <QHash>
#include <QNetworkAccessManager>
#include <QNetworkReply>

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

    virtual void init() {}

    bool yieldToFlowControl() const;
    bool yieldToServer( bool verbose = true ) const;

    void sendRequest( QString api_command, QString body = QLatin1String(), Position *pos = nullptr, quint16 weight = 0 );

    bool isKeyOrSecretUnset() const;
    bool isCommandQueued( const QString &api_command_prefix ) const;
    bool isCommandSent( const QString &api_command_prefix, qint32 min_times = 1 ) const;
    void removeRequest( const QString &api_command, const QString &body );
    void deleteReply( QNetworkReply *const &reply, Request *const &request );

    QString getExchangeStr() const { return exchange_string; }
    QString getExchangeFancyStr() const { return QString( "[REST %1]" ).arg( exchange_string ); }

    QQueue<Request*> nam_queue; // queue for requests so we can load balance timestamp/hmac generation
    QHash<QNetworkReply*,Request*> nam_queue_sent; // request tracking queue

    KeyStore keystore;
    AvgResponseTime avg_response_time;
    QString exchange_string;

    qint64 request_nonce{ 0 }; // nonce (except for trex which uses time atm)
    qint64 last_request_sent_ms{ 0 }; // last nam request time

    qint64 orderbook_update_time{ 0 }; // most recent trade time
    qint64 orderbook_update_request_time{ 0 };
    qint64 ticker_update_time{ 0 };
    qint64 ticker_update_request_time{ 0 };
    qint32 limit_commands_queued{ 35 }; // stop checks if we are over this many commands queued
    qint32 limit_commands_queued_dc_check{ 10 }; // skip dc check if we are over this many commands queued
    qint32 limit_commands_sent{ 35 }; // stop checks if we are over this many commands sent
    qint32 limit_timeout_yield{ 12 };
    qint32 market_cancel_thresh{ 300 }; // limit for market order total for weighting cancels to be sent first

    qint64 slippage_stale_time{ 500 }; // quiet time before we allow an order to be included in slippage price calculations
    qint64 orderbook_stale_tolerance{ 10000 }; // only accept orderbooks sent within this time
    qint64 orders_stale_trip_count{ 0 };
    qint64 books_stale_trip_count{ 0 };

    QTimer *send_timer{ nullptr };
    QTimer *orderbook_timer{ nullptr };
    QTimer *ticker_timer{ nullptr };
    QTimer *timeout_timer{ nullptr };
    QTimer *diverge_converge_timer{ nullptr };

    QNetworkAccessManager *nam{ nullptr };
    Engine *engine{ nullptr };
};

#endif // BASEREST_H

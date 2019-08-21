#ifndef POSITIONMAN_H
#define POSITIONMAN_H

#include "global.h"
#include "coinamount.h"

#include <QObject>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QPair>
#include <QString>

class Position;
class Engine;

//
// PositionMan, helps Engine manage the positions
//
class PositionMan : public QObject
{
    Q_OBJECT

    friend class EngineTest;
    friend class Engine; // TODO: fix this

public:
    explicit PositionMan( Engine *_engine, QObject *parent = nullptr );
    ~PositionMan();

    QSet<Position*> &active() { return positions_active; }
    QSet<Position*> &queued() { return positions_queued; }
    QSet<Position*> &all() { return positions_all; }

    bool hasActivePositions() const;
    bool hasQueuedPositions() const;
    bool isActive( Position *const &pos ) const;
    bool isQueued( Position *const &pos ) const;
    bool isValid( Position *const &pos ) const;
    bool isValidOrderID( const QString &order_id ) const;

    Position *getByOrderID( const QString &order_id ) const;
    Position *getByIndex( const QString &market, const qint32 idx ) const;
    Position *getHighestBuyAll( const QString &market ) const;
    Position *getLowestSellAll( const QString &market ) const;
    Position *getHighestBuyByIndex( const QString &market ) const;
    Position *getHighestSellByIndex( const QString &market ) const;
    Position *getLowestSellByIndex( const QString &market ) const;
    Position *getLowestBuyByIndex( const QString &market ) const;
    Position *getHighestBuyByPrice( const QString &market ) const;
    Position *getLowestSellByPrice( const QString &market ) const;
    Position *getLowestPingPong( const QString &market ) const;
    Position *getHighestPingPong( const QString &market ) const;
    Position *getLowestSpruceBuy( const QString &market ) const;
    Position *getHighestSpruceSell( const QString &market ) const;

    qint32 getLowestPingPongIndex( const QString &market ) const;
    qint32 getHighestPingPongIndex( const QString &market ) const;
    qint32 getMarketOrderTotal( const QString &market, bool onetime_only = false ) const;
    qint32 getBuyTotal( const QString &market ) const;
    qint32 getSellTotal( const QString &market ) const;

    void flipHiBuyPrice( const QString &market, QString tag = QLatin1String() );
    void flipHiBuyIndex( const QString &market, QString tag = QLatin1String() );
    void flipLoSellPrice( const QString &market, QString tag = QLatin1String() );
    void flipLoSellIndex( const QString &market, QString tag = QLatin1String() );

    Coin getLoSell( const QString &market ) const;
    Coin getHiBuy( const QString &market ) const;
    Coin getHiBuyFlipPrice( const QString &market ) const;
    Coin getLoSellFlipPrice( const QString &market ) const;

    QMap<QString,Coin> getActiveSpruceOrdersTotal();
    QMap<QString,Coin> getActiveSpruceOrdersOffset();

    void add( Position *const &pos );
    void activate( Position *const &pos, const QString &order_number );
    void remove( Position *const &pos );

    // ping-pong routines
    void checkBuySellCount();

    // cancel commands
    void cancel( Position *const &pos, bool quiet = false, quint8 cancel_reason = 0 );
    void cancelAll( QString market );
    void cancelLocal( QString market = "" );
    void cancelHighest( const QString &market );
    void cancelLowest( const QString &market );

    bool isDivergingConverging( const QString &market, const qint32 index ) const;
    int getDCCount() { return diverge_converge.size(); }

private:
    void setNextLowest( const QString &market, quint8 side = SIDE_BUY, bool landmark = false );
    void setNextHighest( const QString &market, quint8 side = SIDE_SELL, bool landmark = false );
    void removeFromDC( Position *const &pos );

    // maintain a map of queued positions and set positions
    QHash<QString /* orderNumber */, Position*> positions_by_number;
    QSet<Position*> positions_active; // ptr list of active positions
    QSet<Position*> positions_queued; // ptr list of queued positions
    QSet<Position*> positions_all; // active and queued

    // internal dc stuff
    QMap<QVector<Position*>/*waiting for cancel*/, QPair<bool/*is_landmark*/,QVector<qint32>/*indices*/>> diverge_converge;
    QMap<QString/*market*/, QVector<qint32>/*reserved idxs*/> diverging_converging; // store a vector of converging/diverging indices

    Engine *engine;
};

#endif // POSITIONMAN_H

#ifndef POSITIONMAN_H
#define POSITIONMAN_H

#include "coinamount.h"

#include <QObject>
#include <QMap>
#include <QSet>
#include <QVector>
#include <QPair>
#include <QString>

class Position;
class Engine;

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
    bool isPosition( Position *const &pos ) const;
    bool isPositionOrderID( const QString &order_id ) const;
    Position *getPositionForOrderID( const QString &order_id ) const;
    Position *getPositionByIndex( const QString &market, const qint32 idx ) const;
    Position *getHighestActiveBuyPosByIndex( const QString &market ) const;
    Position *getHighestActiveSellPosByIndex( const QString &market ) const;
    Position *getLowestActiveSellPosByIndex( const QString &market ) const;
    Position *getLowestActiveBuyPosByIndex( const QString &market ) const;
    Position *getHighestActiveBuyPosByPrice( const QString &market ) const;
    Position *getLowestActiveSellPosByPrice( const QString &market ) const;
    Position *getLowestActivePingPong( const QString &market ) const;
    Position *getHighestActivePingPong( const QString &market ) const;
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

    void add( Position *const &pos );
    void activate( Position *const &pos, const QString &order_number );
    void remove( Position *const &pos );
    void removeFromDC( Position *const &pos );
    int getDCCount() { return diverge_converge.size(); }

    bool isDivergingConverging( const QString &market, const qint32 index ) const;

private:
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

#ifndef ALPHATRACKER_H
#define ALPHATRACKER_H

#include "coinamount.h"
#include <QMap>

class Position;

struct AlphaData
{
    Coin v, vp;
    quint64 trades{ 0 };
};

class AlphaTracker
{
public:
    AlphaTracker();

    void addAlpha( const QString &market, Position *pos , bool partial_fill = false );
    void reset();

    Coin getAlpha( const QString &market ) const;
    Coin getVolume( const QString &market ) const;
    Coin getVolumePerTrade( const QString &market ) const;
    Coin getAvgPrice( const QString &market, quint8 side ) const;
    quint64 getTrades( const QString &market ) const;

    void printAlpha() const;
    QString getSaveState() const;
    void readSaveState( const QString &state );

private:
    QList<QString> getMarkets() const;

    QMap<QString,AlphaData> buys, sells;
};

#endif // ALPHATRACKER_H

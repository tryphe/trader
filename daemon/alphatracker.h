#ifndef ALPHATRACKER_H
#define ALPHATRACKER_H

#include "coinamount.h"
#include <QMap>

class Position;

struct AlphaData
{
    Coin getAvgPrice() const { return trades == 0 ? Coin() : vp / trades; }

    Coin v, vp;
    quint64 trades{ 0 };
};

class AlphaTracker
{
public:
    AlphaTracker();
    Coin getAlpha( const QString &market );
    Coin getVolume( const QString &market );
    Coin getVolumePerTrade( const QString &market );
    Coin getAvgPrice( const QString &market, quint8 side );
    quint64 getTrades( const QString &market );
    void addAlpha( const QString &market, Position *pos );
    void reset();

    void printAlpha();
    QString getSaveState();
    void readSaveState( const QString &state );

private:
    QMap<QString,AlphaData> buys, sells;
};

#endif // ALPHATRACKER_H

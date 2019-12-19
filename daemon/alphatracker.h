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

    void reset();

    // "alpha"
    Coin getAlpha( const QString &market ) const;
    Coin getVolume( const QString &market ) const;
    Coin getVolumePerTrade( const QString &market ) const;
    Coin getAvgPrice( const QString &market, quint8 side ) const;
    quint64 getTrades( const QString &market ) const;
    void addAlpha( const QString &market, const quint8 side, const Coin &btc_amount, const Coin &price, bool partial_fill );
    //

    // "daily volume"
    void addDailyVolume( const qint64 epoch_time_secs, const Coin &volume );
    //

    void printAlpha() const;
    void printDailyVolume() const;

    QString getSaveState() const;
    void readSaveState( const QString &state );

private:
    QList<QString> getMarkets() const;

    // "alpha" data
    QMap<QString,AlphaData> buys, sells;

    // "daily volume" data
    qint64 daily_volume_epoch_secs{ 0 }; // the date we started recording volume
    QMap<quint32 /*offset in days from epoch*/, Coin> daily_volume;
};

#endif // ALPHATRACKER_H

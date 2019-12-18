#ifndef SPRUCEOVERSEER_H
#define SPRUCEOVERSEER_H

#include "global.h"
#include "market.h"
#include "coinamount.h"
#include "misctypes.h"

#include <QObject>

class AlphaTracker;
class Spruce;
class Engine;
class QTimer;

class SpruceOverseer : public QObject
{
    Q_OBJECT
public:
    explicit SpruceOverseer();
    ~SpruceOverseer();

    QString getSettingsPath() const { return Global::getTraderPath() + QDir::separator() + "spruce.settings.txt"; }
    void loadSettings();
    void saveSettings();
    void loadStats();
    void saveStats();

    QMap<quint8, Engine*> engine_map;
    AlphaTracker *alpha{ nullptr };
    Spruce *spruce{ nullptr };

signals:
    void gotUserCommandChunk( QString &s ); // loaded settings file

public Q_SLOTS:
    void onSpruceUp();
    void onSaveSpruceSettings();

private:
    void runCancellors( const quint8 side );

    TickerInfo getSpruceSpread( const QString &market, quint8 side );
    TickerInfo getSpruceSpreadLimit( const QString &market, quint8 side );
    TickerInfo getSpreadForMarket( const QString &market );
    Coin getPriceTicksizeForMarket( const QString &market );

    QTimer *spruce_timer{ nullptr };
    QTimer *autosave_timer{ nullptr };
};

#endif // SPRUCEOVERSEER_H

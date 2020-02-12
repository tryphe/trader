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

    friend class CommandRunner;

public:
    explicit SpruceOverseer( Spruce *_spruce );
    ~SpruceOverseer();

    static QString getSettingsPath() { return Global::getTraderPath() + QDir::separator() + "spruce.settings"; }
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
    void runCancellors( const QString &market, const quint8 side, const QString &strategy );

    TickerInfo getSpruceSpreadLimit( const QString &market, quint8 side, bool order_duplicity = false, bool taker_mode = false );
    TickerInfo getSpruceSpread( const QString &market, qint64 *j_ptr = nullptr /*optional inherited spread vibrator*/ , bool order_duplicity = false, bool taker_mode = false, bool spread_collapse = false );
    TickerInfo getSpruceSpreadForSide( const QString &market, quint8 side, bool order_duplicity = false, bool taker_mode = false );
    Coin getPriceTicksizeForMarket( const QString &market );

    QTimer *spruce_timer{ nullptr };
    QTimer *autosave_timer{ nullptr };
};

#endif // SPRUCEOVERSEER_H

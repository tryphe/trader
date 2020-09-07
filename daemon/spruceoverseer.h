#ifndef SPRUCEOVERSEER_H
#define SPRUCEOVERSEER_H

#include "global.h"
#include "market.h"
#include "coinamount.h"
#include "misctypes.h"
#include "diffusionphaseman.h"

#include <QString>
#include <QVector>
#include <QMap>
#include <QDir>
#include <QObject>
#include <QTimer>

class AlphaTracker;
class PriceAggregator;
class SpruceV2;
class EngineMap;
class Engine;
class QTimer;

class SpruceOverseer : public QObject
{
    Q_OBJECT

    friend class SpruceOverseerTest;

public:
    explicit SpruceOverseer( EngineMap *_engine_map, PriceAggregator *_price_aggregator, SpruceV2 *_spruce );
    ~SpruceOverseer();

    static QString getSettingsPath() { return Global::getTraderPath() + QDir::separator() + "spruce.settings"; }
    void loadSettings();
    void saveSettings();
    void loadStats();
    void saveStats();

    const QString &getLastMidspreadPhaseOutput() const { return m_last_midspread_output; }

    EngineMap *engine_map{ nullptr };
    AlphaTracker *alpha{ nullptr };
    PriceAggregator *price_aggregator{ nullptr };
    SpruceV2 *spruce{ nullptr };
    QTimer *spruce_timer{ nullptr };

signals:
    void gotUserCommandChunk( QString &s ); // loaded settings file

public Q_SLOTS:
    void onSpruceUp();
    void onBackupAndSave();

private:
    void runCancellors( Engine *engine, const QString &market, const quint8 side, const QString &strategy, const Coin &flux_price );
    void cancelForReason( Engine *const &engine, const Market &market, const quint8 side, const quint8 reason );

    void adjustSpread( Spread &spread, Coin limit, quint8 side, Coin &default_ticksize, bool expand = true );
    void adjustTicksizeToSpread( Coin &ticksize, Spread &spread, const Coin &ticksize_minimum );
    Spread getSpreadLimit( const QString &market, bool order_duplicity = false );
    Spread getSpreadForSide( const QString &market, quint8 side, bool order_duplicity = false, bool taker_mode = false, bool include_limit_for_side = false, bool is_randomized = false, Coin greed_reduce = Coin() );
    Coin getPriceTicksizeForMarket( const Market &market ) const;

    QMap<QString,Coin> m_last_spread_distance_buys;
    QMap<QString,Coin> m_last_spread_distance_sells;
    QString m_last_midspread_output;

    DiffusionPhaseMan m_phase_man;

    QTimer *autosave_timer{ nullptr };
};

#endif // SPRUCEOVERSEER_H

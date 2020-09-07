#ifndef PRICEAGGREGATOR_H
#define PRICEAGGREGATOR_H

#include "coinamount.h"
#include "misctypes.h"
#include "pricesignal.h"
#include "global.h"

#include <QString>
#include <QVector>
#include <QMap>
#include <QDateTime>
#include <QObject>

class EngineMap;
class QTimer;

/*
 * PriceData
 *
 * Data for each MA in the config
 *
 */
struct PriceData
{
    PriceData() {}
    ~PriceData() {}

    qint64 data_start_secs{ 0 };
    QVector<Coin> data;
};

/* PriceAggregatorConfig
 *
 * Holds data for each market in the PriceAggregator config.
 *
 */
struct PriceAggregatorConfig
{
    PriceAggregatorConfig();
    PriceAggregatorConfig( const QString &_market,
                           const int _base_interval_secs,
                           const int _base_len,
                           const int _strategy_length,
                           const int _strategy_length_internal_option0,
                           const int _interval_counter = 0,
                           const Coin jumpstart_price = Coin() );

    void jumpstart( const Coin &price );

    // data in the config file
    QString market;

    int base_interval_secs{ 0 }; // record new sample for price every x secs
    int base_length{ 0 };
    int strategy_length{ 0 };

    // TODO: these should be a stack and interval/lengths should be in PriceData
    // data in the small_ma file
    PriceData base_data;

    // data in the signal ma file
    PriceSignal signal_base, signal_strategy;
};

/* PriceAggregator
 *
 * Aggregates prices from the engines.
 * Saves/loads price history.
 * Maintains a config of markets to commit to disk.
 * Computes averages, etc. from price history according to config.
 *
 */
class PriceAggregator : public QObject
{
    Q_OBJECT

public:
    explicit PriceAggregator( EngineMap *_engine_map );
    ~PriceAggregator();

    static QString getSettingsPath();
    static QString getSamplesPath( const QString &market, const qint64 interval_secs );

    static void savePriceSamples(const PriceAggregatorConfig &config , const QString filename_override = QString() );
    static bool loadPriceSamples( PriceData &data, const QString &path );

    Spread getSpread( const QString &market ) const;
    Coin getStrategySignal( const QString &market );

    void addPersistentMarket( const PriceAggregatorConfig &config );

    void saveMaybe();
    void save();
    void saveConfig();
    void savePrices();
    void load();
    void loadConfig();
    void loadPrices();

signals:
    void gotUserCommandChunk( QString &s );

public Q_SLOTS:
    void onTimerUp();

private:
    void nextPriceSample();

    QMap<QString, PriceAggregatorConfig> m_config;
    QVector<QString> m_currencies;
    QVector<QString> m_markets;

    qint64 m_last_save_secs{ 0 };
    qint64 m_save_interval_secs{ 3600 }; // save config/samples every x seconds
    EngineMap *m_engine_map{ nullptr };
    QTimer *m_timer{ nullptr };
};

#endif // PRICEAGGREGATOR_H

#ifndef PRICEAGGREGATOR_H
#define PRICEAGGREGATOR_H

#include "coinamount.h"
#include "misctypes.h"
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

    qint64 start_secs{ 0 };
    qint64 data_start_secs{ 0 };
    Signal base_ma;
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
                           const int _small_ma_interval_secs,
                           const int _signal_ma_interval_secs,
                           const int _signal_ma_length,
                           const Coin jumpstart_price = Coin() );

    void jumpstart( const Coin &price );

    // data in the config file
    QString market;
    int small_ma_interval_secs{ 60 }; // record new sample for price every x secs
    int getSmallMALength() const { return signal_ma_interval_secs / small_ma_interval_secs; }
    int signal_ma_interval_secs{ 84600 }; // save price ma every x secs, default 1 day
    int signal_ma_length{ 365 }; // combine this many 1 day ma samples for our signal price

    // TODO: these should be a stack and interval/lengths should be in PriceData
    // data in the small_ma file
    PriceData small_ma;

    // data in the signal ma file
    PriceData signal_ma;
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

    static void savePriceSamples( const QString &market, const qint64 sample_interval, const qint64 data_start_secs, const QVector<Coin> &data );
    static bool loadPriceSamples( PriceData &data, const QString &path, const int ma_length, const int ma_interval );

    Spread getSpread( const QString &market ) const;
    Coin getSignalMA( const QString &market ) const;

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

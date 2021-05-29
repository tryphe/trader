#ifndef SIMULATIONTHREAD_H
#define SIMULATIONTHREAD_H

#include "../daemon/coinamount.h"
#include "../daemon/market.h"
#include "../daemon/misctypes.h"
#include "../daemon/priceaggregator.h"
#include "../daemon/pricesignal.h"
#include "../daemon/sprucev2.h"

#include <QString>
#include <QVector>
#include <QQueue>
#include <QMap>
#include <QObject>
#include <QThread>
#include <QMutex>

struct SignalContainer
{
    ~SignalContainer() { }

    void initSignals( const int signal_count );
    bool isStrategyInitialized();

    PriceSignal price_signal;
    QVector<Coin> price_signal_cache;
    QVector<PriceSignal> strategy_signals;
    qint64 current_idx{ 0 };
};

struct StrategyArgs
{
    StrategyArgs() {}
    StrategyArgs( const PriceSignalType _type, const int _fast, const int _slow, const Coin &_weight )
        : type( _type ),
          length_fast( _fast ),
          length_slow( _slow ),
          weight( _weight )
    {}

    PriceSignalType type{ SMA };

    quint16 length_fast{ 0 }, length_slow{ 0 };
    Coin weight{ CoinAmount::COIN };

    bool isEqualExcludingWeights( const StrategyArgs &other ) const
    {
        return length_fast == other.length_fast &&
               length_slow == other.length_slow &&
               type == other.type;
    }

    bool operator == ( const StrategyArgs &other ) const
    {
        return length_fast == other.length_fast &&
               length_slow == other.length_slow &&
               type == other.type &&
               weight == other.weight;
    }

    operator QString() const { return QString( "%1/%2/%3/%4" )
                                       .arg( type )
                                       .arg( length_fast )
                                       .arg( length_slow )
                                       .arg( weight.toCompact() ); }
};

struct SimulationTask
{
    QByteArray &getUniqueID();

    void addStrategyArgs( const StrategyArgs &new_args ) { m_strategy_args += new_args; }

    // general options
    QVector<QVector<Market>> m_markets_tested;

    QVector<StrategyArgs> m_strategy_args;
    quint8 m_allocation_func{ 0 };

    // results
    QString m_simulation_result;
    QString m_alpha_readout;
    QVector<Coin> m_scores;

    // cached raw bytes of the options, for getRaw()
    QByteArray m_unique_id;
};

class SimulationThread : public QThread
{
    Q_OBJECT

public:
   explicit SimulationThread( const int id = 0 );
   ~SimulationThread();

    int m_id;

    // price data pointers
    QVector<QMap<Market, PriceData>*> m_price_data;

    // current task
    SimulationTask *m_work;

    // external pointers
    QMutex *ext_mutex{ nullptr };
    QVector<SimulationThread*> *ext_threads;
    QVector<SimulationTask*> *ext_work_done{ nullptr }, *ext_work_queued{ nullptr };
    int *ext_work_count_total, *ext_work_count_done, *ext_work_count_started;

private:
    void run() override;
    void runSimulation( const QMap<Market, PriceData> *const &price_data );

    QString m_signals_str;
    QVector<SignalContainer> m_signals;
    PriceSignal m_base_capital_sma0;
    SpruceV2 sp;
    Coin initial_btc_value, highest_btc_value, simulation_cutoff_value, total_volume;
    qint64 m_latest_ts, m_latest_ts_price_cache;
};

#endif // SIMULATIONTHREAD_H

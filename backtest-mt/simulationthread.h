#ifndef SIMULATIONTHREAD_H
#define SIMULATIONTHREAD_H

#include "../daemon/coinamount.h"
#include "../daemon/market.h"
#include "../daemon/misctypes.h"
#include "../daemon/priceaggregator.h"

#include <QString>
#include <QVector>
#include <QMap>
#include <QObject>
#include <QThread>
#include <QMutex>

struct SimulationTask
{
    QByteArray getRaw() const
    {
        QByteArray raw;
        raw += m_samples_start_offset;

        raw += m_strategy_signal_type;
        raw += m_base_ma_length;
        raw += m_rsi_length;
        raw += m_rsi_ma_length;
        raw += m_allocation_func;

        for ( int i = 0; i < m_modulation_length_slow.size(); i++ )
            raw += m_modulation_length_slow.value( i );

        for ( int i = 0; i < m_modulation_length_fast.size(); i++ )
            raw += m_modulation_length_fast.value( i );

        for ( int i = 0; i < m_modulation_factor.size(); i++ )
            raw += m_modulation_factor.value( i );

        for ( int i = 0; i < m_modulation_threshold.size(); i++ )
            raw += m_modulation_threshold.value( i );

        return raw /*QCryptographicHash::hash( raw, QCryptographicHash::Sha256 )*/;
    }

    // general options
    int m_samples_start_offset{ 0 };

    SignalType m_strategy_signal_type{ SMA };
    int m_base_ma_length{ 0 };
    int m_rsi_length{ 0 };
    int m_rsi_ma_length{ 0 };
    int m_allocation_func{ 0 };

    QVector<int> m_modulation_length_slow;
    QVector<int> m_modulation_length_fast;
    QVector<Coin> m_modulation_factor;
    QVector<Coin> m_modulation_threshold;

    // results
    QString m_simulation_result;
    QString m_alpha_readout;
    QMap<int, Coin> m_scores;
};

class SimulationThread : public QThread
{
    Q_OBJECT
public:
   explicit SimulationThread( const int id = 0 );
   ~SimulationThread();

    int m_id, m_work_id;

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
};

#endif // SIMULATIONTHREAD_H

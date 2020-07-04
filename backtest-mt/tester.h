#ifndef TESTER_H
#define TESTER_H

#include "../daemon/priceaggregator.h"
#include "../daemon/market.h"

#include <QString>
#include <QMap>
#include <QObject>
#include <QThread>
#include <QRunnable>
#include <QMutex>

class QTimer;

struct SimulationTask
{
    // general options
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
    QString m_simulation_id;
    QString m_alpha_readout;
    QMap<int, Coin> m_scores;
};


class SimulationThread : public QThread
{
    Q_OBJECT
public:
   explicit SimulationThread();
   ~SimulationThread();

    // price data pointers
    QVector<QMap<Market, PriceData>*> m_price_data;

    // current task
    SimulationTask *m_task;

    // external pointers
    QMutex *ext_mutex{ nullptr };
    QVector<SimulationThread*> *ext_threads;
    QVector<SimulationTask*> *ext_work_done{ nullptr }, *ext_work_queued{ nullptr };
    int *ext_work_count_total, *ext_work_count_done, *ext_work_count_started;

private:
    void run() override;
    void runSimulation( const QMap<Market, PriceData> *const &price_data );
};


class Tester : public QObject
{
    Q_OBJECT

    static const bool RUN_RANDOM_TESTS = true;
    static const int MAX_WORKERS = 8;
    static const int MARKET_VARIATIONS = 2;

public:
    explicit Tester();
    ~Tester();

    void loadPriceDataSingle( const QString &file_name, const Market &market );
    void loadPriceData();

    void generateWork();
    void generateRandomWork();
    void startWork();
    void processFinishedWork();

    void printHighScores( const QMap<Coin, QString> &scores, const int print_count = 3 );
    QString runSimulation();

    /// runSimulation sub steps
    void initPriceData( const qint64 late_start_samples );

public Q_SLOTS:
    void onWorkTimer();

private:
//    QMap<int,QMap<QString, Coin>> m_scores;
    QMap<int, QMap<Coin, QString>> m_highscores;
    QMap<QString, QString> m_alpha_readout;

    // price data
    QVector<QMap<Market, PriceData>> m_price_data_0, m_price_data_1;
    QVector<int> m_price_data_indexes_reserved;

    // work data
    QMutex m_work_mutex;
    int m_work_count_total{ 0 }, m_work_count_done{ 0 }, m_work_count_started{ 0 };
    QVector<SimulationTask*> m_work_queued, m_work_done;
    QTimer *m_work_timer{ nullptr };

    // threads
    QVector<SimulationThread*> m_threads;
};

#endif // TESTER_H

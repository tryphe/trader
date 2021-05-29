#ifndef TESTER_H
#define TESTER_H

#include "../daemon/coinamount.h"
#include "../daemon/market.h"
#include "../daemon/priceaggregator.h"

#include <QString>
#include <QVector>
#include <QMap>
#include <QSet>
#include <QObject>
#include <QMutex>

class QTimer;
class SimulationThread;
class SimulationTask;

class Tester : public QObject
{
    Q_OBJECT

public:
//#define OUTPUT_BASE_CAPITAL
//#define OUTPUT_SIMULATION_TIME
//#define OUTPUT_TOTAL_RUNTIME
//#define OUTPUT_SIMULATION_CUTOFF_WARNING
//#define PRICE_SIGNAL_CACHED_DEF
//#define REPAIR_INDEX_FRAGMENTATION

    static const bool USE_CANDLES_FROM_THIS_DIRECTORY = false; // note: true for realtime, false for backtest
    static const bool OUTPUT_QTY_TARGETS = false;             // note: true for realtime, false for backtest

    static const int MAX_WORKERS = 7;
    static const int MARKET_VARIATIONS = 1;
    static const bool USE_SAVED_WORK = true;
    static const int BASE_INTERVAL = 100; // note: when base interval is 100, signal interval is 1, and vice versa
    static const int CANDLE_INTERVAL_SECS = 300;
    static const int ACTUAL_CANDLE_INTERVAL_SECS = CANDLE_INTERVAL_SECS * BASE_INTERVAL;
    static const int RELATIVE_SATS_TRADED_PER_BASE_INTERVAL = 50000; // note: 50000 for both
    static const int PRICE_SIGNAL_LENGTH = 15;                   // note: 1 for realtime, 15 for backtest
    static const int PRICE_SIGNAL_BIAS = 5;                    // note: -1 for realtime, 5 for backtest
    static const int FEE_DIV = 10000; // every ~2 trades, incur a penalty of base_capital/FEE_DIV on our position

    static const bool WORK_RANDOM = true;
    static const int WORK_RANDOM_TRIES_MAX = 1000000;
    static const bool WORK_INFINITE = true; // regenerate work each RESULTS_OUTPUT_INTERVAL_SECS
    static const int WORK_UNITS_BUFFER = 20000;
    static const int WORK_SAMPLES_START_OFFSET = 0;
    static const bool WORK_SAMPLES_BASE_LEVELER = true; // level the base samples so our latest 5min candle is always used in the current getSignal
    static const bool WORK_PREVENT_RETRY = true; // false = save ram, true = use ram to prevent retry

    static const int RESULTS_HIGH_SCORE_COUNT = 50; // print x best results
    static const int RESULTS_OUTPUT_INTERVAL_SECS = 300; // print and process results every x ms
    static const int RESULTS_OUTPUT_THREAD_SECS = 30; // 0 = print per-thread message every work unit start, >0 = output only every n secs
    static const bool RESULTS_OUTPUT_NEWLY_FINISHED = false;
    static const bool RESULTS_EVICT_ZERO_SCORE = true; // if any score is zero, evict from results map to save resources
    static const bool RESULTS_EVICT_NON_HIGH_SCORE = true; // evict result if not a high score, saves disk/ram

    explicit Tester();
    ~Tester();

    void loadPriceDataSingle( const QString &file_name, const Market &market );
    void loadPriceData();
    void reindexPriceData( PriceData &data, const int interval );

    void fillRandomWorkQueue();
    void generateWork();
    void generateRandomWork();
    void generateWorkFromResultString( const QString &construct );
    void startWork();
    void processFinishedWork();

    void printHighScores( const QMultiMap<Coin, QString> &scores, QTextStream &out, QString description, const int print_count = RESULTS_HIGH_SCORE_COUNT );
    void trimHighScores( QMultiMap<Coin, QString> &scores, QMap<QString, Coin> &scores_by_result );

    void saveFinishedWork();
    void loadFinishedWork();

public Q_SLOTS:
    void onWorkTimer();
    void onThreadFinished();

private:
    QMap<int, QMultiMap<Coin, QString>> m_highscores_by_score; // for each score type, store kv<score,id>
    QMap<int, QMap<QString, Coin>> m_highscores_by_result; // for each score type, store kv<id,score>

    // price data
    QVector<QMap<Market, PriceData>> m_price_data_0;//, m_price_data_1;

    // work data
    QMutex m_work_mutex;
    int m_work_count_total{ 0 }, m_work_count_done{ 0 }, m_work_count_started{ 0 };
    QVector<SimulationTask*> m_work_queued, m_work_done;
    QTimer *m_work_timer{ nullptr };

    // threads
    QVector<SimulationThread*> m_threads;

    // work data, but not accessed by threads
    QSet<QByteArray> m_work_ids_generated_or_done;
    QMap<QByteArray, QString> m_work_results_unsaved;

    // stats
    int m_work_skipped_duplicate{ 0 };
    qint64 m_start_time{ 0 };
};

#endif // TESTER_H

#ifndef TESTER_H
#define TESTER_H

#include "../daemon/coinamount.h"
#include "../daemon/market.h"
#include "../daemon/priceaggregator.h"

#include <QString>
#include <QVector>
#include <QMap>
#include <QObject>
#include <QMutex>

class QTimer;
class SimulationThread;
class SimulationTask;

class Tester : public QObject
{
    Q_OBJECT

    static const int MAX_WORKERS = 8;
    static const int MARKET_VARIATIONS = 2;

    static const bool WORK_RANDOM = true;
    static const int WORK_RANDOM_TRIES = 1000000;
    static const int WORK_SAMPLES_START_OFFSET = 0;

public:
    explicit Tester();
    ~Tester();

    void loadPriceDataSingle( const QString &file_name, const Market &market );
    void loadPriceData();

    void generateWork();
    void generateRandomWork();
    void startWork();
    void processFinishedWork();

    void printHighScores( const QMap<Coin, QString> &scores, QTextStream &out, QString description, const int print_count = 3 );

    void saveFinishedWork();
    void loadFinishedWork();

public Q_SLOTS:
    void onWorkTimer();

private:
    QMap<int, QMultiMap<Coin, QString>> m_highscores_by_score; // for each score type, store kv<score,id>
    QMap<int, QMap<QString, Coin>> m_highscores_by_result; // for each score type, store kv<id,score>
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

    // work data, but not accessed by threads
    QSet<QByteArray> m_work_ids_generated_or_done;
    QMap<QByteArray, QString> m_work_results_saved, m_work_results_unsaved;

    // stats
    int m_work_skipped_duplicate{ 0 };
};

#endif // TESTER_H

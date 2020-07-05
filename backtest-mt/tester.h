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

public Q_SLOTS:
    void onWorkTimer();

private:
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

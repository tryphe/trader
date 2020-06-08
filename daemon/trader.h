#ifndef TREXTRADER_H
#define TREXTRADER_H

#include <QObject>

class QNetworkAccessManager;
class CommandListener;
class CommandRunner;
class AlphaTracker;
class PriceAggregator;
class SpruceV2;
class SpruceOverseer;
class EngineMap;
class Engine;
class TrexREST;
class BncREST;
class PoloREST;
class WavesREST;

class Trader : public QObject
{
    Q_OBJECT

public:
    explicit Trader( QObject *parent = nullptr );
    ~Trader();

public slots:
    void handleCommand( QString &s );
    void handleExitSignal();

private:
    QNetworkAccessManager *nam;

    CommandListener *command_listener{ nullptr };
    CommandRunner *command_runner_trex{ nullptr };
    CommandRunner *command_runner_bnc{ nullptr };
    CommandRunner *command_runner_polo{ nullptr };
    CommandRunner *command_runner_waves{ nullptr };

    AlphaTracker *alpha{ nullptr };
    PriceAggregator *price_aggregator{ nullptr };
    SpruceV2 *spruce{ nullptr };
    SpruceOverseer *spruce_overseer{ nullptr };

    EngineMap *engine_map{ nullptr };
    Engine *engine_trex{ nullptr };
    Engine *engine_bnc{ nullptr };
    Engine *engine_polo{ nullptr };
    Engine *engine_waves{ nullptr };

    TrexREST *rest_trex{ nullptr };
    BncREST *rest_bnc{ nullptr };
    PoloREST *rest_polo{ nullptr };
    WavesREST *rest_waves{ nullptr };
};

#endif // TREXTRADER_H

#ifndef TREXTRADER_H
#define TREXTRADER_H

#include <QObject>

#include "global.h"

class CommandRunner;
class CommandListener;
class FallbackListener;

class Stats;
class Engine;
class REST_OBJECT;

class Trader : public QObject
{
    Q_OBJECT

public:
    explicit Trader( QObject *parent = nullptr );
    ~Trader();

public slots:
    void handleExitSignal();

private:
    CommandRunner *runner;
    CommandListener *listener;
#ifdef FALLBACK_FILE_INPUT
    FallbackListener *listener_fallback;
#endif

    Stats *stats;
    Engine *engine;
    REST_OBJECT *rest;
};

#endif // TREXTRADER_H

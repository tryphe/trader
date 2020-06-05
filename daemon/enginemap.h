#ifndef ENGINEMAP_H
#define ENGINEMAP_H

#include <QMap>

class Engine;

class EngineMap : public QMap<quint8, Engine*>
{
public:
    EngineMap();
};

#endif // ENGINEMAP_H

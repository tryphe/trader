#ifndef ENGINEMAP_H
#define ENGINEMAP_H

#include <QMap>

class Engine;

class EngineMap : public QMap<quint8, Engine*>
{
public:
    explicit EngineMap() {}
};

#endif // ENGINEMAP_H

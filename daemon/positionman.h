#ifndef POSITIONMAN_H
#define POSITIONMAN_H

#include <QObject>


class PositionMan : public QObject
{
    Q_OBJECT

public:
    explicit PositionMan( QObject *parent = nullptr );

signals:

public slots:
};

#endif // POSITIONMAN_H

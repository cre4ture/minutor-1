#ifndef PLAYERINFOS_H
#define PLAYERINFOS_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QDir>

struct PlayerInfo
{
    QString name;
    QVector3D currentPosition;
    bool hasBed;
    QVector3D bedPosition;
};

QVector<PlayerInfo> loadPlayerInfos(QDir path);

#endif // PLAYERINFOS_H

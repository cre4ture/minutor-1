#ifndef PLAYERINFOS_H
#define PLAYERINFOS_H

#include <QString>
#include <QVector>
#include <QVector3D>
#include <QDir>

struct PlayerInfo
{
    PlayerInfo()
        : name("")
        , currentPosition()
        , hasBed(false)
        , bedPosition()
        , dimention(0)
    {

    }

    QString name;
    QVector3D currentPosition;
    bool hasBed;
    QVector3D bedPosition;
    int dimention;

    bool isOverworld() const { return dimention == 0; }
    bool isNether() const { return dimention == -1; }
    bool isEnd() const { return dimention == 1; }
};

QVector<PlayerInfo> loadPlayerInfos(QDir path);

#endif // PLAYERINFOS_H

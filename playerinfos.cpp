#include "playerinfos.h"
#include "nbt.h"

#include <QDirIterator>

QVector<PlayerInfo> loadPlayerInfos(QDir path)
{
    QVector<PlayerInfo> players;

    if (path.cd("playerdata") || path.cd("players")) {
        QDirIterator it(path);
        bool hasPlayers = false;
        while (it.hasNext()) {
            it.next();
            if (it.fileInfo().isFile()) {
                hasPlayers = true;
                NBT player(it.filePath());
                auto pos = player.at("Pos");
                double posX = pos->at(0)->toDouble();
                double posY = pos->at(1)->toDouble();
                double posZ = pos->at(2)->toDouble();
#if 0
                auto dim = player.at("Dimension");
                if (dim && (dim->toInt() == -1)) {
                    posX *= 8;
                    posY *= 8;
                    posZ *= 8;
                }
#endif
                QString playerName = it.fileInfo().completeBaseName();
                QRegExp id("[0-9a-z]{8,8}\\-[0-9a-z]{4,4}\\-[0-9a-z]{4,4}"
                           "\\-[0-9a-z]{4,4}\\-[0-9a-z]{12,12}");
                if (id.exactMatch(playerName)) {
                    playerName = QString("Player %1").arg(players.length());
                }

                PlayerInfo info;
                info.name = playerName;
                info.currentPosition.setX(static_cast<float>(posX));
                info.currentPosition.setY(static_cast<float>(posY));
                info.currentPosition.setZ(static_cast<float>(posZ));

                info.hasBed = player.has("SpawnX");
                if (info.hasBed)
                {
                    info.bedPosition.setX(static_cast<float>(player.at("SpawnX")->toDouble()));
                    info.bedPosition.setY(static_cast<float>(player.at("SpawnY")->toDouble()));
                    info.bedPosition.setZ(static_cast<float>(player.at("SpawnZ")->toDouble()));
                }

                players.append(info);
            }
        }
    }

    return players;
}

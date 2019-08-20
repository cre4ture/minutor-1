#include "nbt.h"
#include "properties.h"
#include "chunkloader.h"

#include <QApplication>

#include <iostream>

void printUsage(const char* appname)
{
    std::cout << "usage " << appname << " nbt|chunk <nbt-filename>|<leveldir> [pos_x] [pos_z]" << std::endl;
}

int main(int argc, char* argv[])
{
    const char* const appname = argv[0];

    QApplication app(argc, argv);

    if (argc <= 2)
    {
        printUsage(appname);
        return -1;
    }

    QString type = argv[1];
    QString path = argv[2];

    Properties p;

    if (type == "nbt")
    {
        NBT nbt(path);

        p.DisplayProperties(nbt.getRoot()->getData());

        p.setWindowTitle(path);
    }
    else
    if (type == "chunk")
    {
        if (argc <= 4)
        {
            printUsage(appname);
            return -1;
        }

        QString str_x = argv[3];
        QString str_z = argv[4];

        bool ok_x = true;
        int x = str_x.toInt(&ok_x);

        if (!ok_x)
        {
            std::cout << str_x.toStdString() << " is not a valid number!" << std::endl;
            return -1;
        }

        bool ok_z = true;
        int z = str_z.toInt(&ok_z);

        if (!ok_z)
        {
            std::cout << str_z.toStdString() << " is not a valid number!" << std::endl;
            return -1;
        }

        auto chunkId = ChunkID::fromCoordinates(x,z);

        ChunkLoader loader(path, chunkId);

        std::cout << "loading chunk from region file: " << loader.getRegionFilename(path, chunkId).toStdString() << std::endl;

        auto nbt = loader.loadNbt();

        if (nbt != nullptr)
        {
            p.DisplayProperties(nbt->getRoot()->getData());

            p.setWindowTitle(path + " chunk " + QString::number(chunkId.getX()) + "," + QString::number(chunkId.getZ()));
        }
    }

    p.showNormal();

    return app.exec();
}

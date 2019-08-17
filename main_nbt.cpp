#include "nbt.h"
#include "properties.h"

#include <QApplication>

#include <iostream>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    if (argc <= 1)
    {
        std::cout << "usage " << argv[0] << " <nbt-filename>" << std::endl;
        return -1;
    }

    QString nbtfile = argv[1];

    NBT nbt(nbtfile);

    Properties p;

    p.DisplayProperties(nbt.getRoot()->getData());

    p.showNormal();

    p.setWindowTitle(nbtfile);

    return app.exec();
}

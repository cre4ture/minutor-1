/** Copyright (c) 2013, Sean Kasun */

#include "./chunkloader.h"
#include "./chunkcache.h"
#include "./chunk.h"

ChunkLoader::ChunkLoader(QString path, int x, int z,
                         const QMap<ChunkID, Chunk *> &cache,
                         QMutex *mutex) : path(path), x(x), z(z),
  cache(cache), mutex(mutex) {
}
ChunkLoader::~ChunkLoader() {
}

void ChunkLoader::run() {

    const bool success = runInternal();
    if (success)
    {
        emit loaded(x, z);
    }

    emit chunkUpdated(success, x, z);
}

bool ChunkLoader::runInternal()
{
    int rx = x >> 5;
    int rz = z >> 5;

    QFile f(path + "/region/r." + QString::number(rx) + "." +
            QString::number(rz) + ".mca");
    if (!f.open(QIODevice::ReadOnly)) {  // no chunks in this region
      return false;
    }
    // map header into memory
    uchar *header = f.map(0, 4096);
    int offset = 4 * ((x & 31) + (z & 31) * 32);
    int coffset = (header[offset] << 16) | (header[offset + 1] << 8) |
        header[offset + 2];
    int numSectors = header[offset+3];
    f.unmap(header);

    if (coffset == 0) {  // no chunk
      f.close();
      return false;
    }

    uchar *raw = f.map(coffset * 4096, numSectors * 4096);
    if (raw == nullptr) {
      f.close();
      return false;
    }
    NBT nbt(raw);
    ChunkID id(x, z);
    mutex->lock();
    Chunk *chunk = cache[id];
    if (chunk)
      chunk->load(nbt);
    mutex->unlock();
    f.unmap(raw);
    f.close();

    return true;
}

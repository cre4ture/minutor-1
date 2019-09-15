/** Copyright (c) 2013, Sean Kasun */

#include "./chunkloader.h"
#include "./chunkcache.h"
#include "./chunk.h"

#include <future>

ChunkLoader::ChunkLoader(QString path, ChunkID id_)
  : path(path)
  , id(id_)
{}

ChunkLoader::~ChunkLoader()
{}

void ChunkLoader::run() {

    auto newChunk = runInternal();
    if (newChunk)
    {
        //emit loaded(id.getX(), id.getZ());
    }

    //emit chunkUpdated(newChunk, id);
}

QSharedPointer<NBT> ChunkLoader::loadNbt()
{
    QString filename = getRegionFilename(path, id);

    QFile f(filename);
  if (!f.open(QIODevice::ReadOnly)) {  // no chunks in this region
      return nullptr;
  }
  // map header into memory
  uchar *header = f.map(0, 4096);
  int offset = 4 * ((id.getX() & 31) + (id.getZ() & 31) * 32);
  int coffset = (header[offset] << 16) | (header[offset + 1] << 8) |
      header[offset + 2];
  int numSectors = header[offset+3];
  f.unmap(header);

  if (coffset == 0) {  // no chunk
    f.close();
    return nullptr;
  }

  uchar *raw = f.map(coffset * 4096, numSectors * 4096);
    if (raw == nullptr) {
    f.close();
    return nullptr;
  }

  auto nbt = QSharedPointer<NBT>::create(raw);

  f.unmap(raw);
  f.close();

  return nbt;
}

QSharedPointer<Chunk> ChunkLoader::runInternal()
{
    auto nbt = loadNbt();

    if (nbt == nullptr)
    {
        return nullptr;
    }

    auto chunk = QSharedPointer<Chunk>::create();
    chunk->load(*nbt);

    return chunk;
}

QString ChunkLoader::getRegionFilename(const QString& path, const ChunkID& id)
{
    int rx = id.getX() >> 5;
    int rz = id.getZ() >> 5;

    return path + "/region/r." + QString::number(rx) + "." +
            QString::number(rz) + ".mca";
}

void ChunkLoaderThreadPool::enqueueChunkLoading(QString path, ChunkID id)
{
    m_queue.push([this, path, id](){
        ChunkLoader loader(path, id);
        auto chunk = loader.runInternal();
        emit chunkUpdated(chunk, id);
    });
}

void ChunkLoaderThreadPool::signalUpdated(QSharedPointer<Chunk> chunk, ChunkID id)
{
    emit chunkUpdated(chunk, id);
}

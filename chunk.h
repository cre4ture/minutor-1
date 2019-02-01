/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNK_H_
#define CHUNK_H_

#include "./nbt.h"
#include "./entity.h"

#include <QtCore>
#include <QVector>

class BlockIdentifier;
class ChunkRenderer;
class DrawHelper2;

class ChunkSection {
 public:
  quint16 getBlock(int x, int y, int z);
  quint16 getBlock(int offset, int y);
  quint8  getData(int x, int y, int z);
  quint8  getData(int offset, int y);
  quint8  getLight(int x, int y, int z);
  quint8  getLight(int offset, int y);

  quint16 blocks[4096];
  quint8  data[2048];
  quint8  light[2048];
};

struct Block
{
    Block()
        : id(0)
        , bd(0)
    {}

    quint32 id;
    quint32 bd; // blockdata?
};

class Chunk {
 public:
  typedef QMap<QString, QSharedPointer<OverlayItem>> EntityMap;
  Chunk();
  void load(const NBT &nbt);
  ~Chunk();

  const EntityMap& getEntityMap() const { return entities; }

  Block getBlockData(int x, int y, int z) const;

  int getChunkX() const { return chunkX; }
  int getChunkZ() const { return chunkZ; }

 protected:

  quint8 biomes[256];
  int highest;
  ChunkSection *sections[16];
  int renderedAt;
  int renderedFlags;
  bool loaded;
  uchar image[16 * 16 * 4];  // cached render
  uchar depth[16 * 16];
  EntityMap entities;
  int chunkX;
  int chunkZ;
  friend class MapView;
  friend class ChunkRenderer;
  friend class ChunkCache;
  friend class WorldSave;
  friend class DrawHelper2;
};

#endif  // CHUNK_H_

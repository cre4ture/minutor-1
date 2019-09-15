/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNK_H_
#define CHUNK_H_

#include "./nbt.h"
#include "./entity.h"

#include <QtCore>
#include <QVector>

#include "./paletteentry.h"
#include "./generatedstructure.h"

class BlockIdentifier;
class ChunkRenderer;
class DrawHelper2;

class ChunkSection {
 public:
  const PaletteEntry & getPaletteEntry(int x, int y, int z);
  const PaletteEntry & getPaletteEntry(int offset, int y);
  quint8 getSkyLight(int x, int y, int z);
  quint8 getSkyLight(int offset, int y);
  quint8 getBlockLight(int x, int y, int z);
  quint8 getBlockLight(int offset, int y);

  PaletteEntry *palette;
  int        paletteLength;

  std::array<quint16, 16*16*16> blocks;
//quint8  skyLight[16*16*16/2];   // not needed in Minutor
  std::array<quint8, 16*16*16/2> blockLight;
};

struct Block
{
    Block()
        : id(0)
    {}

    quint32 id;
};

class Chunk : public QObject {
  Q_OBJECT

 public:
  typedef QMap<QString, QSharedPointer<OverlayItem>> EntityMap;
  Chunk();
  ~Chunk();
  void load(const NBT &nbt);

  const EntityMap& getEntityMap() const { return entities; }

  Block getBlockData(int x, int y, int z) const;

  int getChunkX() const { return chunkX; }
  int getChunkZ() const { return chunkZ; }

 protected:
  void loadSection1343(ChunkSection *cs, const Tag *section);
  void loadSection1519(ChunkSection *cs, const Tag *section);

  quint32 biomes[16*16];
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

  QList<QSharedPointer<GeneratedStructure> > structurelist;

  friend class MapView;
  friend class ChunkRenderer;
  friend class ChunkCache;
  friend class WorldSave;
  friend class DrawHelper2;
};

#endif  // CHUNK_H_

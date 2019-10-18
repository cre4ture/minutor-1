/** Copyright (c) 2013, Sean Kasun */
#ifndef CHUNK_H_
#define CHUNK_H_

#include "./nbt.h"
#include "./entity.h"

#include <QtCore>
#include <QVector>
#include <QImage>

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

class RenderedChunk;

class Chunk : public QObject {
  Q_OBJECT

 public:
  typedef QMap<QString, QSharedPointer<OverlayItem>> EntityMap;
  Chunk();
  ~Chunk();
  void load(const NBT &nbt);

  const EntityMap& getEntityMap() const { return *entities; }
  const QSharedPointer<EntityMap> getEntityMapSp() const { return entities; }

  Block getBlockData(int x, int y, int z) const;

  int getChunkX() const { return chunkX; }
  int getChunkZ() const { return chunkZ; }

 protected:
  void loadSection1343(ChunkSection *cs, const Tag *section);
  void loadSection1519(ChunkSection *cs, const Tag *section);

  quint32 biomes[16*16];
  int highest;
  ChunkSection *sections[16];
  bool loaded;
  QSharedPointer<EntityMap> entities;
  int chunkX;
  int chunkZ;

  QSharedPointer<RenderedChunk> rendered;

  QList<QSharedPointer<GeneratedStructure> > structurelist;

  friend class MapView;
  friend class ChunkRenderer;
  friend class ChunkCache;
  friend class WorldSave;
  friend class DrawHelper2;
};

struct RenderParams
{
  RenderParams()
    : renderedAt(-1)
    , renderedFlags(0)
  {}

  int renderedAt;
  int renderedFlags;

  bool operator==(const RenderParams& other) const
  {
    return (renderedAt == other.renderedAt) && (renderedFlags == other.renderedFlags);
  }

  bool operator!=(const RenderParams& other) const
  {
    return !(operator==(other));
  }
};

class RenderedChunk
{
public:
  const QWeakPointer<Chunk> chunk;
  QSharedPointer<Chunk::EntityMap> entities;
  const int chunkX;
  const int chunkZ;

  RenderParams renderedFor;

  QImage depth;
  QImage image;

  RenderedChunk(const QSharedPointer<Chunk>& chunk)
    : chunk(chunk)
    , entities(chunk->getEntityMapSp())
    , chunkX(chunk->getChunkX())
    , chunkZ(chunk->getChunkZ())
    , renderedFor()
  {
  }

  void freeImageData()
  {
    depth = QImage();
    image = QImage();
  }
};

#endif  // CHUNK_H_

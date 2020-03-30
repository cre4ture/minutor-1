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

#include <array>

class BlockIdentifier;
class ChunkRenderer; 
class DrawHelper2;

class ChunkSection {
 public:
  const PaletteEntry & getPaletteEntry(int x, int y, int z) const;
  const PaletteEntry & getPaletteEntry(int offset, int y) const;
  quint8 getSkyLight(int x, int y, int z);
  quint8 getSkyLight(int offset, int y);
  quint8 getBlockLight(int x, int y, int z) const;
  quint8 getBlockLight(int offset, int y) const;

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
  Chunk();
  ~Chunk();
  void load(const NBT &nbt);

  typedef QMap<QString, QSharedPointer<OverlayItem>> EntityMap;

  qint32 get_biome(int x, int z);
  qint32 get_biome(int x, int y, int z);
  qint32 get_biome(int offset);

  const EntityMap& getEntityMap() const { return *entities; }
  const QSharedPointer<EntityMap> getEntityMapSp() const { return entities; }

  const ChunkSection* getSectionByY(int y) const;

  uint getBlockHid(int x, int y, int z) const;
  Block getBlockData(int x, int y, int z) const;

  int getChunkX() const { return chunkX; } 
  int getChunkZ() const { return chunkZ; }


 protected:
  void loadSection1343(ChunkSection *cs, const Tag *section);
  void loadSection1519(ChunkSection *cs, const Tag *section);

  int version;
  qint32 biomes[16 * 16 * 4];
  int highest;
  std::array<ChunkSection*, 16> sections;
  bool loaded;
  QSharedPointer<EntityMap> entities;
  int chunkX;
  int chunkZ;

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

  void invalidate()
  {
    renderedAt = -1;
  }

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
  QSharedPointer<Chunk::EntityMap> entities;
  const int chunkX;
  const int chunkZ;

  RenderParams renderedFor;

  QImage depth;
  QImage image;

  RenderedChunk(const QSharedPointer<Chunk>& chunk)
    : entities(chunk->getEntityMapSp())
    , chunkX(chunk->getChunkX())
    , chunkZ(chunk->getChunkZ())
    , renderedFor()
  {
  }

  void init();

  void freeImageData()
  {
    depth = QImage();
    image = QImage();
  }
};

#endif  // CHUNK_H_

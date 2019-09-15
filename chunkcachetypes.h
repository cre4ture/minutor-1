#ifndef CHUNKCACHETYPES_H
#define CHUNKCACHETYPES_H

#include "location.h"

#include <QSharedPointer>
#include <QMap>

// ChunkID is the key used to identify entries in the Cache
// Chunks are identified by their coordinates (CX,CZ) but a single key is needed to access a map like structure
class ChunkID {
 public:
    ChunkID();
  ChunkID(int cx, int cz);
  bool operator==(const ChunkID &) const;
  bool operator<(const ChunkID&) const;
  friend uint qHash(const ChunkID &);

  int getX() const { return cx; }
  int getZ() const { return cz; }

  static ChunkID fromCoordinates(int x, int z)
  {
      if (x < 0) x -= 16;
      if (z < 0) z -= 16;
      return ChunkID(x / 16, z / 16);
  }

  Location toCoordinates() const
  {
    int cx = cx*16 + 8;
    int cz = cz*16 + 8;
    return Location(cx, cz);
  }

 protected:
  int cx, cz;
};

class Chunk;

//typedef QMap<ChunkID, QSharedPointer<Chunk> > ChunkCacheT;

#endif // CHUNKCACHETYPES_H

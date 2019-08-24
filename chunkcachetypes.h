#ifndef CHUNKCACHETYPES_H
#define CHUNKCACHETYPES_H

#include "location.h"

#include <QSharedPointer>
#include <QMap>

class ChunkID {
 public:
    ChunkID();
  ChunkID(int x, int z);
  bool operator==(const ChunkID &) const;
  bool operator<(const ChunkID&) const;
  friend uint qHash(const ChunkID &);

  int getX() const { return x; }
  int getZ() const { return z; }

  static ChunkID fromCoordinates(int x, int z)
  {
      if (x < 0) x -= 16;
      if (z < 0) z -= 16;
      return ChunkID(x / 16, z / 16);
  }

  Location toCoordinates() const
  {
    int cx = x*16 + 8;
    int cz = z*16 + 8;
    return Location(cx, cz);
  }

 protected:
  int x, z;
};

class Chunk;

//typedef QMap<ChunkID, QSharedPointer<Chunk> > ChunkCacheT;

#endif // CHUNKCACHETYPES_H

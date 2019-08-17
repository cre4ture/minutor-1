#ifndef CHUNKCACHETYPES_H
#define CHUNKCACHETYPES_H

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
      return ChunkID(x / 16, z / 16);
  }

 protected:
  int x, z;
};

class Chunk;

//typedef QMap<ChunkID, QSharedPointer<Chunk> > ChunkCacheT;

#endif // CHUNKCACHETYPES_H

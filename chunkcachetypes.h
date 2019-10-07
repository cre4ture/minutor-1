#ifndef CHUNKCACHETYPES_H
#define CHUNKCACHETYPES_H

#include "location.h"

#include <QSharedPointer>
#include <QMap>

class CoordinateID
{
public:
  CoordinateID() : cx(0), cz(0)
  {}

  CoordinateID(int cx, int cz) : cx(cx), cz(cz)
  {}

  int getX() const { return cx; }
  int getZ() const { return cz; }

  bool operator==(const CoordinateID &other) const {
    return (other.cx == cx) && (other.cz == cz);
  }

  bool operator<(const CoordinateID &other) const {
      return (other.cx < cx) || ((other.cx == cx) && (other.cz < cz));
  }

protected:
  int cx, cz;
};

// ChunkID is the key used to identify entries in the Cache
// Chunks are identified by their coordinates (CX,CZ) but a single key is needed to access a map like structure
template <int _SizeN>
class ChunkID_t: public CoordinateID
{
 public:
  using CoordinateID::CoordinateID;

  enum {
    SIZE_N = _SizeN
  };

  static ChunkID_t fromCoordinates(int x, int z)
  {
      if (x < 0) x -= SIZE_N;
      if (z < 0) z -= SIZE_N;
      return ChunkID_t(x / SIZE_N, z / SIZE_N);
  }

  Location toCoordinates() const
  {
    int cx = cx*SIZE_N + (SIZE_N / 2);
    int cz = cz*SIZE_N + (SIZE_N / 2);
    return Location(cx, cz);
  }
};

class ChunkID: public ChunkID_t<16>
{
public:
  using ChunkID_t::ChunkID_t;
};

class ChunkGroupID: public ChunkID_t<2>
{
public:
  using ChunkID_t::ChunkID_t;
};

inline uint qHash(const CoordinateID &c) {
  return (c.getX() << 16) ^ (c.getZ() & 0xffff);  // safe way to hash a pair of integers
}

#endif // CHUNKCACHETYPES_H

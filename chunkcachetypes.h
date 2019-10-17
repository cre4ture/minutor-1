#ifndef CHUNKCACHETYPES_H
#define CHUNKCACHETYPES_H

#include "location.h"

#include <QSharedPointer>
#include <QMap>
#include <QSize>

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
template <int _SizeN, typename _ThisT>
class ChunkID_t: public CoordinateID
{
 public:
  using CoordinateID::CoordinateID;

  enum {
    SIZE_N = _SizeN
  };

  static _ThisT fromCoordinates(double x, double z)
  {
      return _ThisT(floor(x / SIZE_N), floor(z / SIZE_N));
  }

  Location centerCoordinates() const
  {
    int x = cx*SIZE_N + (SIZE_N / 2);
    int z = cz*SIZE_N + (SIZE_N / 2);
    return Location(x, z);
  }

  Location topLeft() const
  {
    int x = cx*SIZE_N;
    int z = cz*SIZE_N;
    return Location(x, z);
  }
};

class ChunkID: public ChunkID_t<16, ChunkID>
{
public:
  using ChunkID_t::ChunkID_t;

  static QSize getSize()
  {
    const int size1d = ChunkID::SIZE_N;
    const QSize size2d(size1d, size1d);
    return size2d;
  }
};

class ChunkGroupID: public ChunkID_t<16, ChunkGroupID>
{
public:
  using ChunkID_t::ChunkID_t;

  static QSize getSize()
  {
    const int size1d = ChunkID::SIZE_N * ChunkGroupID::SIZE_N;
    const QSize size2d(size1d, size1d);
    return size2d;
  }
};

inline uint qHash(const CoordinateID &c) {
  return (c.getX() << 16) ^ (c.getZ() & 0xffff);  // safe way to hash a pair of integers
}

class ChunkIteratorC
{
public:
  ChunkIteratorC()
    : cx(0)
    , cz(0)
  {}

  void setRange(int newRangeX, int newRangeZ)
  {
    range_x = newRangeX;
    range_z = newRangeZ;
  }

  ChunkID getNext(int startx, int startz)
  {
    cx++;
    if (cx >= range_x)
    {
      cx = 0;
      cz++;
    }

    if (cz >= range_z)
    {
      cx = 0;
      cz = 0;
    }

    return ChunkID(startx + cx, startz + cz);
  }

private:
  int cx;
  int cz;
  int range_x;
  int range_z;
};

#endif // CHUNKCACHETYPES_H

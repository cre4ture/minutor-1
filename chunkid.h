#ifndef CHUNKID_H
#define CHUNKID_H

#include "coordinateid.h"

// ChunkID is the key used to identify entries in the Cache
// Chunks are identified by their coordinates (CX,CZ) but a single key is needed to access a map like structure

// ChunkID is the key used to identify entries in the Cache
// Chunks are identified by their coordinates (CX,CZ) but a single key is needed to access a map like structure
template <int _SizeN, typename _ThisT = CoordinateID>
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

  CoordinateID centerCoordinates() const
  {
    int x = cx*SIZE_N + (SIZE_N / 2);
    int z = cz*SIZE_N + (SIZE_N / 2);
    return CoordinateID(x, z);
  }

  CoordinateID topLeft() const
  {
    int x = cx*SIZE_N;
    int z = cz*SIZE_N;
    return CoordinateID(x, z);
  }

  CoordinateID bottomRight() const
  {
    int x = cx*SIZE_N + SIZE_N;
    int z = cz*SIZE_N + SIZE_N;
    return CoordinateID(x, z);
  }

  static CoordinateID relativeCoordinate(const CoordinateID& coordinate)
  {
    return CoordinateID(static_cast<unsigned int>(coordinate.getX()) % SIZE_N,
                        static_cast<unsigned int>(coordinate.getZ()) % SIZE_N);
  }

  static size_t relativeIndex(const CoordinateID& coordinate)
  {
    auto relCoords = relativeCoordinate(coordinate);
    return relCoords.getZ() * SIZE_N + relCoords.getX();
  }

  RectangleIterator begin() const
  {
    const auto tl = topLeft();
    return RectangleIterator(QRect(tl.getX(), tl.getZ(), SIZE_N, SIZE_N));
  }

  RectangleIterator end() const
  {
    return begin().end();
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


#endif // CHUNKID_H

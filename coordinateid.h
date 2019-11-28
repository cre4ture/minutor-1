#ifndef CHUNKCACHETYPES_H
#define CHUNKCACHETYPES_H

#include "location.h"

#include <QSharedPointer>
#include <QMap>
#include <QSize>
#include <QRect>

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

  bool operator!=(const CoordinateID &other) const {
    return !(operator==(other));
  }

  bool operator<(const CoordinateID &other) const {
      return (other.cx < cx) || ((other.cx == cx) && (other.cz < cz));
  }

  CoordinateID operator+(const CoordinateID &other) const {
    CoordinateID result(*this);
    result.cx += other.cx;
    result.cz += other.cz;
    return result;
  }

protected:
  int cx, cz;
};

inline size_t convertTo16Bit(int value)
{
  const size_t uval = static_cast<size_t>(value);
  return (uval >> 16) + uval;
}

inline uint qHash(const CoordinateID &c) {

  static_assert(sizeof(int) == 4, "");
  static_assert(sizeof(size_t) == 8, "");
  static_assert(sizeof(c.getX()) == 4, "");
  static_assert(sizeof(c.getX()) == sizeof(uint), "");
  static_assert(std::numeric_limits<uint>::max() == 0xFFFFFFFF, "");

  const int halfbits = 16;
  const size_t n1 = convertTo16Bit(c.getX());
  const size_t n2 = convertTo16Bit(c.getZ());
  uint result = 0;
  for (size_t i = 0; i < halfbits; i++)
  {
    size_t bitA = (n1 & (1 << i)) << (i+0);
    size_t bitB = (n2 & (1 << i)) << (i+1);

    result |= bitA;
    result |= bitB;
  }

  return result;
}

class RectangleIterator: public CoordinateID
{
public:
  RectangleIterator(const QRect& rect_)
    : CoordinateID(rect_.left(), rect_.top())
    , rect(rect_)
  {}

  RectangleIterator end()
  {
    RectangleIterator it(*this);
    it.cx = rect.left();
    it.cz = rect.bottom()+1;
    return it;
  }

  RectangleIterator& operator++()
  {
    cx++;
    if (cx > rect.right())
    {
      cx = rect.left();
      cz++;
    }

    return *this;
  }

  const CoordinateID operator*() const
  {
    return (*this);
  }

private:
  const QRect rect;
};

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

  std::pair<int, int> getNext(int startx, int startz)
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

    return std::pair<int, int>(startx + cx, startz + cz);
  }

private:
  int cx;
  int cz;
  int range_x;
  int range_z;
};

#endif // CHUNKCACHETYPES_H

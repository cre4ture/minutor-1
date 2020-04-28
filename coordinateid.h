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

#endif // CHUNKCACHETYPES_H

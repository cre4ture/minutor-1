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

class RectangleInnerToOuterIterator
{
public:
  RectangleInnerToOuterIterator(const QRect& rect_)
    : rect(rect_)
    , center(rect_.center())
    , currentPos(0,0)
    , shellNr(0)
    , subNr(0)
    , stepNr(0)
  {
    reset();
  }

  void reset()
  {
    currentPos = CoordinateID(center.x(), center.y());
    shellNr = 1;
    subNr = 1;
    stepNr = 0;
  }

  RectangleInnerToOuterIterator end()
  {
    RectangleInnerToOuterIterator it(*this);
    it.stepNr = rect.size().width() * rect.size().height();
    return it;
  }

  RectangleInnerToOuterIterator& operator++()
  {
    stepNr++;

    do {
      inc();
      getCoordinate();
    } while(((*this) != end()) && !rect.contains(QPoint(currentPos.getX(), currentPos.getZ())));

    return (*this);
  }

  const CoordinateID& operator*() const
  {
    return currentPos;
  }

  const CoordinateID* operator->() const
  {
    return &currentPos;
  }

  bool operator==(const RectangleInnerToOuterIterator& other) const
  {
    return (stepNr == other.stepNr);
  }

  bool operator!=(const RectangleInnerToOuterIterator& other) const
  {
    return !operator==(other);
  }

  const QRect& currentRect() const
  {
    return rect;
  }

private:
  QRect rect;
  QPoint center;
  CoordinateID currentPos;
  int shellNr;
  int subNr;
  int stepNr;

  void inc()
  {
    subNr++;
    const int lastValidSub = (((shellNr-1) * 2) + 1);
    if (subNr > lastValidSub)
    {
      shellNr++;
      subNr=1;
    }
  }

  void getCoordinate()
  {
    const int shellSign   = ((shellNr & 0x1) != 0) ? -1 : 1;
    const int shellRadius = (shellNr >> 1);

    const bool subNrBit1      = ((subNr & 0x1) != 0);
    const int  subNrRemaining = (subNr >> 1);

    int ncx = center.x() + (shellRadius * shellSign);
    int ncz = center.y() + (shellRadius * shellSign);

    if (subNrBit1)
    {
      ncz -= subNrRemaining * shellSign;
    }
    else
    {
      ncx -= subNrRemaining * shellSign;
    }

    currentPos = CoordinateID(ncx, ncz);
  }
};

class ChunkIteratorC
{
public:
  ChunkIteratorC()
    : it(QRect())
  {}

  void setRange(int newRangeX, int newRangeZ, bool forceRestart)
  {
    if (forceRestart ||
        newRangeX != it.currentRect().width() ||
        newRangeZ != it.currentRect().height())
    {
      it = RectangleInnerToOuterIterator(QRect(0, 0, newRangeX, newRangeZ));
    }
  }

  std::pair<int, int> getNext(int startx, int startz)
  {
    std::pair<int, int> pos(startx + it->getX(), startz + it->getZ());

    ++it;
    if (it == it.end())
    {
      it.reset();
    }

    return pos;
  }

private:
  RectangleInnerToOuterIterator it;
};

#endif // CHUNKCACHETYPES_H

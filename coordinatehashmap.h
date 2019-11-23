#ifndef CHUNKHASHMAP_H
#define CHUNKHASHMAP_H

#include "coordinateid.h"

template<class _ValueT, int reduction = 16>
class CoordinateHashMap
{
public:
  using KeyT = CoordinateID;
  using KeyValueT = ChunkID_t<reduction, CoordinateID>;
  enum
  {
    ArraySize = reduction * reduction
  };
  using ArrayValueT = std::array<_ValueT, ArraySize>;
  using BaseT = QHash<CoordinateID, ArrayValueT>;

  _ValueT& operator[](const KeyT& key)
  {
    const CoordinateID internalKey = KeyValueT::fromCoordinates(key.getX(), key.getZ());
    ArrayValueT& valueblock = rawmap[internalKey];
    const size_t subId = KeyValueT::relativeIndex(key);
    return valueblock[subId];
  }

  void reserve(int asize)
  {
    const int blockedSize = asize / ArraySize;
    rawmap.reserve(blockedSize);
  }

  void clear()
  {
    rawmap.clear();
  }

private:
  QHash<CoordinateID, std::array<_ValueT, reduction * reduction> > rawmap;
};

#endif // CHUNKHASHMAP_H

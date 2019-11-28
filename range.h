#ifndef RANGE_H
#define RANGE_H

#include <utility>
#include <limits>

template <typename _ValueT>
class Range
{
  public:
  Range(_ValueT start_including, _ValueT end_including)
    : start(start_including)
      , end(end_including)
  {}

  static Range createFromUnorderedParams(_ValueT v1, _ValueT v2)
  {
    if (v1 > v2)
    {
      std::swap(v1, v2);
    }

    return Range(v1, v2);
  }

  static Range max()
  {
    return Range(std::numeric_limits<_ValueT>::lowest(), std::numeric_limits<_ValueT>::max());
  }

  const _ValueT start;
  const _ValueT end;

  bool isInsideRange(_ValueT value) const
  {
    return (value >= start) && (value <= end);
  }
};

#endif // RANGE_H

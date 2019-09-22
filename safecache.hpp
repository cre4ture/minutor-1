#ifndef SAFECACHE_HPP
#define SAFECACHE_HPP

#include <QCache>
#include <QSharedPointer>

template<class _keyT, class _valueT>
class SafeCache
{
public:
  SafeCache() {}

  void clear()
  {
    unsafeCache.clear();
  }

  int totalCost() const
  {
    return unsafeCache.totalCost();
  }

  int maxCost() const
  {
    return unsafeCache.maxCost();
  }

  void setMaxCost(int m)
  {
    unsafeCache.setMaxCost(m);
  }

  void insert(const _keyT& key, const _valueT& value)
  {
    unsafeCache.insert(key,
                       new QSharedPointer<_valueT>(
                         QSharedPointer<_valueT>::create(_valueT(value))
                         )
                       );
  }

  void insert(const _keyT& key, const QSharedPointer<_valueT>& value)
  {
    unsafeCache.insert(key,
                       new QSharedPointer<_valueT>(value)
                       );
  }

  QSharedPointer<_valueT> operator[](const _keyT& key)
  {
    QSharedPointer<_valueT>* p_sp = unsafeCache[key];
    if (p_sp)
    {
      return *p_sp;
    }
    else
    {
      return nullptr;
    }
  }

private:
  QCache<_keyT, QSharedPointer<_valueT> > unsafeCache;
};

#endif // SAFECACHE_HPP

#ifndef SAFECACHE_HPP
#define SAFECACHE_HPP

#include <QCache>
#include <QSharedPointer>

//#define SAFE_CACHE_USE_QHASH

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
#ifdef SAFE_CACHE_USE_QHASH
    return 1;
#else
    return unsafeCache.totalCost();
#endif
  }

  int maxCost() const
  {
#ifdef SAFE_CACHE_USE_QHASH
    return 1;
#else
    return unsafeCache.maxCost();
#endif
  }

  void setMaxCost(int m)
  {
#ifndef SAFE_CACHE_USE_QHASH
    unsafeCache.setMaxCost(m);
#endif
  }


#ifdef SAFE_CACHE_USE_QHASH
  void insert(const _keyT& key, const QSharedPointer<_valueT>& value)
  {
    unsafeCache.insert(key, value);
  }

  QSharedPointer<_valueT> operator[](const _keyT& key)
  {
    return unsafeCache[key];
  }

  QSharedPointer<_valueT> findOrCreate(const _keyT& key)
  {
    QSharedPointer<_valueT> p_sp = unsafeCache[key];
    if (p_sp)
    {
      return p_sp;
    }
    else
    {
      auto sptr = QSharedPointer<_valueT>::create();
      insert(key, sptr);
      return sptr;
    }
  }

#else
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

  QSharedPointer<_valueT> findOrCreate(const _keyT& key)
  {
    QSharedPointer<_valueT>* p_sp = unsafeCache[key];
    if (p_sp)
    {
      return *p_sp;
    }
    else
    {
      auto sptr = QSharedPointer<_valueT>::create();
      insert(key, sptr);
      return sptr;
    }
  }
#endif

private:
#ifdef SAFE_CACHE_USE_QHASH
  QHash<_keyT, QSharedPointer<_valueT> > unsafeCache;
#else
  QCache<_keyT, QSharedPointer<_valueT> > unsafeCache;
#endif
};

#endif // SAFECACHE_HPP

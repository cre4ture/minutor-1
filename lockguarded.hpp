#ifndef LOCKGUARDED_HPP
#define LOCKGUARDED_HPP

#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>

template<class GuardedT>
class LockGuarded
{
public:
  LockGuarded()
    : protectedInstance()
  {}

  LockGuarded(LockGuarded&& instance)
    : protectedInstance(instance)
  {}

  class Lock
  {
  public:
    Lock() = delete;

    explicit Lock(LockGuarded& parent_)
      : parent(parent_)
      , lock(QSharedPointer<QMutexLocker>::create(&parent_.mutex))
    {}

    Lock(const Lock&) = delete;

    Lock(Lock&& moveFrom)
      : parent(moveFrom.parent)
      , lock(std::move(moveFrom.lock))
    {}

    GuardedT& operator()() const
    {
      return parent.protectedInstance;
    }

  private:
    LockGuarded& parent;
    QSharedPointer<QMutexLocker> lock;
  };

  Lock lock()
  {
    return Lock(*this);
  }

private:
  QMutex mutex;
  GuardedT protectedInstance;
};

#endif // LOCKGUARDED_HPP

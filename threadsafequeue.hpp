#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>

class ThreadSafeQueueBase
{
public:
  ThreadSafeQueueBase()
    : alive_(true)
  {}

  void signalTerminate()
  {
    {
      std::unique_lock<std::mutex> mlock(mutex_);
      alive_ = false;
    }
    cond_.notify_all();
  }

  bool isAlive() const
  {
    return alive_;
  }

protected:
  class ProtectedBase
  {
  public:
    ProtectedBase(ThreadSafeQueueBase& self_)
      : self(self_)
      , mlock(self.mutex_)
    {
    }

  protected:
    ThreadSafeQueueBase& self;
    std::unique_lock<std::mutex> mlock;
  };

  class ConstProtectedBase
  {
  public:
    ConstProtectedBase(const ThreadSafeQueueBase& self_)
      : self(self_)
      , mlock(self.mutex_)
    {
    }

  protected:
    const ThreadSafeQueueBase& self;
    std::unique_lock<std::mutex> mlock;
  };

  class ProtectedPop: public ProtectedBase
  {
  public:
    using ProtectedBase::ProtectedBase;

    void waitNextEvent()
    {
      self.cond_.wait(mlock);
    }
  };

  class ProtectedPush: public ProtectedBase
  {
  public:
    using ProtectedBase::ProtectedBase;

    ~ProtectedPush()
    {
      mlock.unlock(); // notify should be after mutex unlock!
      self.cond_.notify_one();
    }
  };

private:
  bool alive_;
  mutable std::mutex mutex_;
  std::condition_variable cond_;
};

template <typename T>
class ThreadSafeQueue: public ThreadSafeQueueBase
{
public:
  bool pop(T& item)
  {
    ProtectedPop guard(*this);

    while (queue_.empty() && isAlive())
    {
      guard.waitNextEvent();
    }

    bool endReached = queue_.empty();
    if (!endReached)
    {
      item = queue_.front();
      queue_.pop_front();
    }

    return !endReached;
  }

  bool tryPop(T& item)
  {
    ProtectedPop guard(*this);

    bool endReached = queue_.empty();
    if (!endReached)
    {
      item = queue_.front();
      queue_.pop_front();
    }

    return !endReached;
  }

  size_t push(const T& item, bool back = true)
  {
    ProtectedPush guard(*this);

    if(back)
      queue_.push_back(item);
    else
      queue_.push_front(item);

    return queue_.size();
  }

  size_t push(T&& item, bool back = true)
  {
    ProtectedPush guard(*this);

    if(back)
      queue_.push_back(std::move(item));
    else
      queue_.push_front(std::move(item));

    return queue_.size();
  }

  size_t getCurrentQueueLength() const
  {
    ConstProtectedBase guard(*this);
    return queue_.size();
  }

private:
  std::deque<T> queue_;
};

template <typename T, typename PrioT = int>
class ThreadSafePriorityQueueBase: public ThreadSafeQueueBase
{
public:
  size_t push(const T& item, const PrioT& priority)
  {
    ProtectedPush guard(*this);

    queue_.emplace(priority, item);

    return queue_.size();
  }

  size_t push(T&& item, const PrioT& priority)
  {
    ProtectedPush guard(*this);

    queue_.emplace(priority, std::move(item));

    return queue_.size();
  }

  size_t getCurrentQueueLength() const
  {
    ConstProtectedBase guard(*this);
    return queue_.size();
  }

protected:
  std::multimap<PrioT, T> queue_;
};

template <typename T, typename PrioT = int>
class ThreadSafePriorityQueue: public ThreadSafePriorityQueueBase<T, PrioT>
{
public:
  using BaseT = ThreadSafePriorityQueueBase<T, PrioT>;

  bool pop(T& item)
  {
    typename BaseT::ProtectedPop guard(*this);

    while (BaseT::queue_.empty() && BaseT::isAlive())
    {
      guard.waitNextEvent();
    }

    bool endReached = BaseT::queue_.empty();
    if (!endReached)
    {
      auto it = BaseT::queue_.begin();
      item = it->second;
      BaseT::queue_.erase(it);
    }

    return !endReached;
  }
};

template <typename T, typename PrioT = int>
class ThreadSafePriorityQueueWithIdleJob: public ThreadSafePriorityQueueBase<T, PrioT>
{
public:
  using BaseT = ThreadSafePriorityQueueBase<T, PrioT>;

  bool pop(T& item)
  {
    typename BaseT::ProtectedPop guard(*this);

    while (BaseT::queue_.empty() && defaultValueQueue_.empty() && BaseT::isAlive())
    {
      guard.waitNextEvent();
    }

    bool endReached = BaseT::queue_.empty();
    if (!endReached)
    {
      auto it = BaseT::queue_.begin();
      item = it->second;
      BaseT::queue_.erase(it);
      return true;
    }
    else
    {
      endReached = defaultValueQueue_.empty();
      if (!endReached)
      {
        auto it = defaultValueQueue_.begin();
        if (it->second)
        {
          item = *(it->second);
        }
        return true;
      }
    }

    return !endReached;
  }

  void registerDefaultValue(const std::shared_ptr<T>& item, PrioT priority)
  {
    typename BaseT::ProtectedPush guard(*this);

    defaultValueQueue_.emplace(priority, item);
  }

  void unregisterDefaultValue(const std::shared_ptr<T>& item)
  {
    typename BaseT::ProtectedBase guard(*this);

    for (auto it = defaultValueQueue_.begin(); it != defaultValueQueue_.end(); ++it)
    {
      if (it->second == item)
      {
        defaultValueQueue_.erase(it);
        return;
      }
    }
  }

private:
  std::multimap<PrioT, std::shared_ptr<T> > defaultValueQueue_;
};

#endif // THREADSAFEQUEUE_HPP

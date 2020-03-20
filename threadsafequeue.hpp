#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>

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

  template<class SelfT>
  class ProtectedBase
  {
  public:
    ProtectedBase(SelfT& self_)
      : self(self_)
      , mlock(self.mutex_)
    {
    }

  protected:
    SelfT& self;
    std::unique_lock<std::mutex> mlock;
  };

  template<class SelfT>
  class ConstProtectedBase
  {
  public:
    ConstProtectedBase(const SelfT& self_)
      : self(self_)
      , mlock(self.mutex_)
    {
    }

  protected:
    const SelfT& self;
    std::unique_lock<std::mutex> mlock;
  };

  template<class SelfT>
  class ProtectedPop: public ProtectedBase<SelfT>
  {
  public:
    using BaseT = ProtectedBase<SelfT>;
    using BaseT::BaseT;

    void waitNextEvent()
    {
      BaseT::self.cond_.wait(BaseT::mlock);
    }
  };

  template<class SelfT>
  class ProtectedPush: public ProtectedBase<SelfT>
  {
  public:
    using BaseT = ProtectedBase<SelfT>;
    using BaseT::BaseT;

    ~ProtectedPush()
    {
      BaseT::mlock.unlock(); // notify should be after mutex unlock!
      BaseT::self.cond_.notify_one();
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
  using ThisT = ThreadSafeQueue;

  bool pop(T& item)
  {
    ProtectedPop<ThisT> guard(*this);

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
    ProtectedPop<ThisT> guard(*this);

    bool endReached = queue_.empty();
    if (!endReached)
    {
      item = queue_.front();
      queue_.pop_front();
    }

    return !endReached;
  }

  template<class SelfT>
  class ProtectedPush2: public ProtectedPush<SelfT>
  {
  public:
    using BaseT = ProtectedPush<SelfT>;
    using BaseT::BaseT;

    void push(T&& item, bool back = true)
    {
      if(back)
        BaseT::self.queue_.push_back(std::move(item));
      else
        BaseT::self.queue_.push_front(std::move(item));
    }

    void push(const T& item, bool back = true)
    {
      if(back)
        BaseT::self.queue_.push_back(item);
      else
        BaseT::self.queue_.push_front(item);
    }
  };


  size_t push(const T& item, bool back = true)
  {
    ProtectedPush2<ThisT> guard(*this);

    guard.push(item, back);

    return queue_.size();
  }

  size_t push(T&& item, bool back = true)
  {
    ProtectedPush2<ThisT> guard(*this);

    guard.push(std::move(item), back);

    return queue_.size();
  }

  size_t getCurrentQueueLength() const
  {
    ConstProtectedBase<ThisT> guard(*this);
    return queue_.size();
  }

private:
  std::deque<T> queue_;
};

template <typename T, typename PrioT = int>
class ThreadSafePriorityQueueBase: public ThreadSafeQueueBase
{
public:
  using ThisT = ThreadSafePriorityQueueBase;

  size_t push(const T& item, const PrioT& priority)
  {
    ProtectedPush<ThisT> guard(*this);

    queue_.emplace(priority, item);

    return queue_.size();
  }

  size_t push(T&& item, const PrioT& priority)
  {
    ProtectedPush<ThisT> guard(*this);

    queue_.emplace(priority, std::move(item));

    return queue_.size();
  }

  size_t getCurrentQueueLength() const
  {
    ConstProtectedBase<ThisT> guard(*this);
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
  using ThisT = ThreadSafePriorityQueueWithIdleJob;
  using BaseT = ThreadSafePriorityQueueBase<T, PrioT>;

  bool pop_n(size_t n, std::vector<T>& listOut)
  {
    typename BaseT::template ProtectedPop<ThisT> guard(*this);

    while (BaseT::queue_.empty() && defaultValueQueue_.empty() && BaseT::isAlive())
    {
      guard.waitNextEvent();
    }

    for (size_t i = 0; (i < n) && (!BaseT::queue_.empty()); i++)
    {
      auto it = BaseT::queue_.begin();
      listOut.push_back(it->second);
      BaseT::queue_.erase(it);
    }

    if (listOut.size() > 0)
    {
      return true;
    }
    else
    {
      bool endReached = defaultValueQueue_.empty();
      if (!endReached)
      {
        auto it = defaultValueQueue_.begin();
        if (it->second)
        {
          listOut.push_back(*(it->second));
        }
        return true;
      }
    }

    return false;
  }

  bool pop(T& item)
  {
    typename BaseT::template ProtectedPop<ThisT> guard(*this);

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
    typename BaseT::template ProtectedPush<ThisT> guard(*this);

    defaultValueQueue_.emplace(priority, item);
  }

  void unregisterDefaultValue(const std::shared_ptr<T>& item)
  {
    typename BaseT::template ProtectedBase<ThisT> guard(*this);

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

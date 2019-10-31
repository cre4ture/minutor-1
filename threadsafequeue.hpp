#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

#include <deque>
#include <thread>
#include <mutex>
#include <condition_variable>

template <typename T>
class ThreadSafeQueue
{
 public:
    ThreadSafeQueue()
        : alive_(true)
    {}

    bool pop(T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        while (queue_.empty() && alive_)
        {
            cond_.wait(mlock);
        }

        bool result = (queue_.size() > 0);
        if (result)
        {
            item = queue_.front();
            queue_.pop_front();
        }

        return result;
    }

    size_t push(const T& item, bool back = true)
    {
      size_t currentSize;
      {
        std::unique_lock<std::mutex> mlock(mutex_);

        if(back)
          queue_.push_back(item);
        else
          queue_.push_front(item);

        currentSize = queue_.size();
      }
      cond_.notify_one();
      return currentSize;
    }

    size_t push(T&& item, bool back = true)
    {
      size_t currentSize;
      {
        std::unique_lock<std::mutex> mlock(mutex_);

        if(back)
          queue_.push_back(std::move(item));
        else
          queue_.push_front(std::move(item));

        currentSize = queue_.size();
      }
      cond_.notify_one();
      return currentSize;
    }

    void signalTerminate()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        alive_ = false;
        mlock.unlock();
        cond_.notify_all();
    }

    size_t getCurrentQueueLength() const
    {
      std::unique_lock<std::mutex> mlock(mutex_);
      return queue_.size();
    }

    private:
        bool alive_;
        std::deque<T> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cond_;
};

#endif // THREADSAFEQUEUE_HPP

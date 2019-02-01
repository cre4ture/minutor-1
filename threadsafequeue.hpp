#ifndef THREADSAFEQUEUE_HPP
#define THREADSAFEQUEUE_HPP

#include <queue>
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
            queue_.pop();
        }

        return result;
    }

    void push(const T& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        queue_.push(item);
        mlock.unlock();
        cond_.notify_one();
    }

    void push(T&& item)
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        queue_.push(std::move(item));
        mlock.unlock();
        cond_.notify_one();
    }

    void signalTerminate()
    {
        std::unique_lock<std::mutex> mlock(mutex_);
        alive_ = false;
        mlock.unlock();
        cond_.notify_all();
    }

    private:
        bool alive_;
        std::queue<T> queue_;
        std::mutex mutex_;
        std::condition_variable cond_;
};

#endif // THREADSAFEQUEUE_HPP

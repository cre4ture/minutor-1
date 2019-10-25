#ifndef ASYNCTASKPROCESSORBASE_HPP
#define ASYNCTASKPROCESSORBASE_HPP

#include "threadsafequeue.hpp"

#include <QSharedPointer>

class AsyncTaskProcessorBase
{
public:
    typedef std::function<void()> JobT;

    AsyncTaskProcessorBase();

    size_t enqueueJob(const JobT& job)
    {
      return m_queue.push(job);
    }

    size_t getQueueLength() const
    {
      return m_queue.getCurrentQueueLength();
    }

protected:
    class ImplC;

    QSharedPointer<ImplC> m_impl;
    ThreadSafeQueue<JobT>& m_queue;
};

#endif // ASYNCTASKPROCESSORBASE_HPP

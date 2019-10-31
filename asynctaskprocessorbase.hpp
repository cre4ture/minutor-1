#ifndef ASYNCTASKPROCESSORBASE_HPP
#define ASYNCTASKPROCESSORBASE_HPP

#include "threadsafequeue.hpp"

#include <QSharedPointer>

class AsyncTaskProcessorBase
{
public:
    typedef std::function<void()> JobT;

    AsyncTaskProcessorBase();

    size_t enqueueJob(const JobT& job, bool back = true)
    {
      return m_queue.push(job, back);
    }

    size_t getQueueLength() const
    {
      return m_queue.getCurrentQueueLength();
    }

private:
    class ImplC;

    QSharedPointer<ImplC> m_impl;
    ThreadSafeQueue<JobT>& m_queue;
};

#endif // ASYNCTASKPROCESSORBASE_HPP

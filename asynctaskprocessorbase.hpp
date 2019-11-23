#ifndef ASYNCTASKPROCESSORBASE_HPP
#define ASYNCTASKPROCESSORBASE_HPP

#include "threadsafequeue.hpp"

#include <QSharedPointer>

class AsyncTaskProcessorBase
{
public:
    typedef std::function<void()> JobT;

    AsyncTaskProcessorBase();

    enum class JobPrio
    {
      low,
      high
    };

    size_t enqueueJob(const JobT& job, JobPrio prio = JobPrio::low)
    {
      return m_queue.push(job, prio == JobPrio::low ? true : false);
    }

    size_t getQueueLength() const
    {
      return m_queue.getCurrentQueueLength();
    }

    size_t getNumberOfThreads() const;

private:
    class ImplC;

    QSharedPointer<ImplC> m_impl;
    ThreadSafeQueue<JobT>& m_queue;
};

#endif // ASYNCTASKPROCESSORBASE_HPP

#ifndef ASYNCTASKPROCESSORBASE_HPP
#define ASYNCTASKPROCESSORBASE_HPP

#include "threadsafequeue.hpp"
#include "jobprio.h"

#include <QSharedPointer>


class PriorityThreadPool
{
public:
    using JobT = std::function<void()>;
    using QueueType = ThreadSafePriorityQueueWithIdleJob<JobT, JobPrio>;

    PriorityThreadPool();

    size_t enqueueJob(const JobT& job, JobPrio prio = JobPrio::low)
    {
      return m_queue.push(job, prio);
    }

    size_t getQueueLength() const
    {
      return m_queue.getCurrentQueueLength();
    }

    size_t getNumberOfThreads() const;

    QueueType& queueSink()
    {
      return m_queue;
    }

private:
    class HiddenImplementationC;

    QSharedPointer<HiddenImplementationC> m_impl;
    QueueType& m_queue;
};

#endif // ASYNCTASKPROCESSORBASE_HPP

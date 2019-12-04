#ifndef ASYNCTASKPROCESSORBASE_HPP
#define ASYNCTASKPROCESSORBASE_HPP

#include "threadsafequeue.hpp"
#include "jobprio.h"

#include <QSharedPointer>


class PriorityThreadPool
{
public:
    typedef std::function<void()> JobT;

    PriorityThreadPool();

    size_t enqueueJob(const JobT& job, JobPrio prio = JobPrio::low)
    {
      return m_queue.push(job, static_cast<int>(prio));
    }

    size_t getQueueLength() const
    {
      return m_queue.getCurrentQueueLength();
    }

    size_t getNumberOfThreads() const;

private:
    class HiddenImplementationC;
    using QueueType = ThreadSafePriorityQueue<JobT>;

    QSharedPointer<HiddenImplementationC> m_impl;
    QueueType& m_queue;
};

#endif // ASYNCTASKPROCESSORBASE_HPP

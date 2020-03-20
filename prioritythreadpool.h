#ifndef ASYNCTASKPROCESSORBASE_HPP
#define ASYNCTASKPROCESSORBASE_HPP

#include "threadsafequeue.hpp"
#include "prioritythreadpoolinterface.h"

#include <QSharedPointer>

class PriorityThreadPoolN: public PriorityThreadPoolInterface
{
public:
  using QueueType = ThreadSafePriorityQueueWithIdleJob<JobT, JobPrio>;

  PriorityThreadPoolN(size_t threadCount);

  size_t enqueueJob(const JobT& job, JobPrio prio = JobPrio::low) override
  {
    return m_queue.push(job, prio);
  }

  size_t getQueueLength() const
  {
    return m_queue.getCurrentQueueLength();
  }

  size_t getNumberOfThreads() const override;

  QueueType& queueSink()
  {
    return m_queue;
  }

private:
  class HiddenImplementationC;

  QSharedPointer<HiddenImplementationC> m_impl;
  QueueType& m_queue;
};

class PriorityThreadPool: public PriorityThreadPoolN
{
public:
  PriorityThreadPool();
};

#endif // ASYNCTASKPROCESSORBASE_HPP

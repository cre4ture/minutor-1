#ifndef PRIORITYTHREADPOOLINTERFACE_H
#define PRIORITYTHREADPOOLINTERFACE_H

#include "jobprio.h"

#include <functional>

class PriorityThreadPoolInterface
{
public:
  using JobT = std::function<void()>;

  virtual ~PriorityThreadPoolInterface() {}

  virtual size_t enqueueJob(const JobT& job, JobPrio prio = JobPrio::low) = 0;

  virtual size_t getNumberOfThreads() const = 0;
};


#endif // PRIORITYTHREADPOOLINTERFACE_H

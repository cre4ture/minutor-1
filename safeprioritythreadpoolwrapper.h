#ifndef SAFEPRIORITYTHREADPOOLWRAPPER_H
#define SAFEPRIORITYTHREADPOOLWRAPPER_H

#include "prioritythreadpoolinterface.h"
#include "cancellation.hpp"

#include <memory>

class SafePriorityThreadPoolWrapper: public PriorityThreadPoolInterface
{
public:
  SafePriorityThreadPoolWrapper(PriorityThreadPoolInterface& actualThreadPool);

  virtual size_t enqueueJob(const JobT &job, JobPrio prio = JobPrio::low) override;
  virtual size_t getNumberOfThreads() const override;

  void renewCancellation();

  auto getCancelToken()
  {
    return cancellation->getToken();
  }

private:
  PriorityThreadPoolInterface& actualThreadPool;
  std::shared_ptr<AsyncExecutionGuardAndAccessor_t<SafePriorityThreadPoolWrapper> > cancellation;

};

#endif // SAFEPRIORITYTHREADPOOLWRAPPER_H

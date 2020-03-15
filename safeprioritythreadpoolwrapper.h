#ifndef SAFEPRIORITYTHREADPOOLWRAPPER_H
#define SAFEPRIORITYTHREADPOOLWRAPPER_H

#include "prioritythreadpoolinterface.h"
#include "cancellationasyncexecutionguard.h"

#include <memory>

class SafePriorityThreadPoolWrapper
{
public:
  SafePriorityThreadPoolWrapper(PriorityThreadPoolInterface& actualThreadPool);

  template<typename _FuncT>
  size_t enqueueJob(const CancellationTokenWeakPtr& cancellationToken,
                    const _FuncT &job,
                    JobPrio prio = JobPrio::low)
  {
    return actualThreadPool.enqueueJob([cancellationToken, job]()
      {
        auto guard = cancellationToken.createExecutionGuardChecked();
        job(guard);
      }, prio);
  }

private:
  PriorityThreadPoolInterface& actualThreadPool;
};

class SimpleSafePriorityThreadPoolWrapper
{
public:
  SimpleSafePriorityThreadPoolWrapper(PriorityThreadPoolInterface& actualThreadPool_);

  template<typename _FuncT>
  size_t enqueueJob(const _FuncT &job,
                    JobPrio prio = JobPrio::low)
  {
    return safeWrapper.enqueueJob(cancellation->getTokenPtr(), job, prio);
  }

  void renewCancellation();

  CancellationTokenWeakPtr getCancelToken();

private:
  SafePriorityThreadPoolWrapper safeWrapper;
  std::unique_ptr<AsyncExecutionCancelGuard> cancellation;

};

#endif // SAFEPRIORITYTHREADPOOLWRAPPER_H

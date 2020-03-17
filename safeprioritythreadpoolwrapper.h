#ifndef SAFEPRIORITYTHREADPOOLWRAPPER_H
#define SAFEPRIORITYTHREADPOOLWRAPPER_H

#include "prioritythreadpoolinterface.h"
#include "cancellationasyncexecutionguard.h"

#include <memory>

// enforces the use of a cancellationToken / ExecutionSatusToken
class SafePriorityThreadPoolWrapper
{
public:
  SafePriorityThreadPoolWrapper(PriorityThreadPoolInterface& actualThreadPool);

  template<typename _FuncT>
  size_t enqueueJob(const ExecutionStatusToken& cancellationToken,
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

// automatically attaches and uses a cancellationToken / ExecutionSatusToken
// in combination with AsyncExecutionCancelGuard that automatically cancells and waits for cancellation done when destroyed
class SimpleSafePriorityThreadPoolWrapper: public SafePriorityThreadPoolWrapper
{
  using BaseT = SafePriorityThreadPoolWrapper;

public:
  SimpleSafePriorityThreadPoolWrapper(PriorityThreadPoolInterface& actualThreadPool_);

  template<typename _FuncT>
  size_t enqueueJob(const _FuncT &job,
                    JobPrio prio = JobPrio::low)
  {
    return BaseT::enqueueJob(cancellation->getTokenPtr(), job, prio);
  }

  void renewCancellation();

  ExecutionStatusToken getCancelToken();

private:
  std::unique_ptr<AsyncExecutionCancelGuard> cancellation;     // must be last member

};

#endif // SAFEPRIORITYTHREADPOOLWRAPPER_H

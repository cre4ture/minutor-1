#ifndef SAFEPRIORITYTHREADPOOLWRAPPER_H
#define SAFEPRIORITYTHREADPOOLWRAPPER_H

#include "prioritythreadpoolinterface.h"
#include "cancellationasyncexecutionguard.h"

#include <memory>

// enforces the use of a cancellationToken / ExecutionSatusToken
class SafePriorityThreadPoolWrapper
{
public:
  SafePriorityThreadPoolWrapper(PriorityThreadPoolInterface& actualThreadPool_)
    : actualThreadPool(actualThreadPool_)
  {}

  template<typename _FuncT, typename DataT = NullData>
  size_t enqueueJob(const ExecutionStatusToken_t<DataT>& cancellationToken,
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
template<class DataT>
class SimpleSafePriorityThreadPoolWrapper_t: public SafePriorityThreadPoolWrapper
{
  using BaseT = SafePriorityThreadPoolWrapper;

public:
  SimpleSafePriorityThreadPoolWrapper_t(PriorityThreadPoolInterface& actualThreadPool_)
    : BaseT(actualThreadPool_)
    , cancellation(std::make_unique<AsyncExecutionCancelGuard_t<DataT> >())
  {}


  template<typename _FuncT>
  size_t enqueueJob(const _FuncT &job,
                    JobPrio prio = JobPrio::low)
  {
    return BaseT::enqueueJob(cancellation->getTokenPtr(), job, prio);
  }

  void renewCancellation()
  {
    cancellation = std::make_unique<AsyncExecutionCancelGuard_t<DataT> >();
  }

  std::unique_ptr<AsyncExecutionCancelGuard_t<DataT> > renewCancellationAndReturnOld()
  {
    std::unique_ptr<AsyncExecutionCancelGuard_t<DataT> > result = std::move(cancellation);
    renewCancellation();
    return result;
  }

  ExecutionStatusToken_t<DataT> getCancelToken()
  {
    return cancellation->getTokenPtr();
  }

private:
  std::unique_ptr<AsyncExecutionCancelGuard_t<DataT> > cancellation;     // must be last member

};

typedef SimpleSafePriorityThreadPoolWrapper_t<NullData> SimpleSafePriorityThreadPoolWrapper;

#endif // SAFEPRIORITYTHREADPOOLWRAPPER_H

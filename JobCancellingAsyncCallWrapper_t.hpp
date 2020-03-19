#ifndef JOBCANCELLINGASYNCCALLWRAPPER_T_HPP
#define JOBCANCELLINGASYNCCALLWRAPPER_T_HPP

#include "executionstatus.h"

// enforces the use of a cancellationToken / ExecutionSatusToken
template<class ThreadPoolT, typename... ArgumentsT>
class JobCancellingAsyncCallWrapper_t
{
public:
  JobCancellingAsyncCallWrapper_t(ThreadPoolT& actualThreadPool_)
    : actualThreadPool(actualThreadPool_)
  {}

  template<typename _FuncT, typename DataT = NullData>
  size_t enqueueJob(const ExecutionStatusToken_t<DataT>& cancellationToken,
                    const _FuncT &job, ArgumentsT... args)
  {
    return actualThreadPool.enqueueJob([cancellationToken, job]()
      {
        auto guard = cancellationToken.createExecutionGuardChecked();
        job(guard);
      }, args...);
  }

private:
  ThreadPoolT& actualThreadPool;
};

#endif // JOBCANCELLINGASYNCCALLWRAPPER_T_HPP

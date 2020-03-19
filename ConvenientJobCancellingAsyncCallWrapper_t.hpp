#ifndef CONVENIENTJOBCANCELLINGASYNCCALLWRAPPER_T_HPP
#define CONVENIENTJOBCANCELLINGASYNCCALLWRAPPER_T_HPP

#include "JobCancellingAsyncCallWrapper_t.hpp"
#include "cancellationasyncexecutionguard.h"

// automatically attaches and uses a cancellationToken / ExecutionSatusToken
// in combination with AsyncExecutionCancelGuard that automatically cancells and waits for cancellation done when destroyed
template<class DataT, class ThreadPoolT, typename... ArgumentsT>
class ConvenientJobCancellingAsyncCallWrapper_t: public JobCancellingAsyncCallWrapper_t<ThreadPoolT, ArgumentsT...>
{
  using BaseT = JobCancellingAsyncCallWrapper_t<ThreadPoolT, ArgumentsT...>;

public:
  ConvenientJobCancellingAsyncCallWrapper_t(ThreadPoolT& actualThreadPool_)
    : BaseT(actualThreadPool_)
    , cancellation(std::make_unique<AsyncExecutionCancelGuard_t<DataT> >())
  {}


  template<typename _FuncT>
  size_t enqueueJob(const _FuncT &job,
                    ArgumentsT... args)
  {
    return BaseT::enqueueJob(cancellation->getTokenPtr(), job, args...);
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

#endif // CONVENIENTJOBCANCELLINGASYNCCALLWRAPPER_T_HPP

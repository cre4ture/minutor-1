#include "safeprioritythreadpoolwrapper.h"

SafePriorityThreadPoolWrapper::SafePriorityThreadPoolWrapper(PriorityThreadPoolInterface &actualThreadPool_)
  : actualThreadPool(actualThreadPool_)
  , cancellation(std::make_shared<AsyncExecutionGuardAndAccessor_t<SafePriorityThreadPoolWrapper> >(std::ref(*this)))
{

}

size_t SafePriorityThreadPoolWrapper::enqueueJob(const PriorityThreadPoolInterface::JobT &job, JobPrio prio)
{
  return actualThreadPool.enqueueJob([cancel = cancellation->getWeakAccessor(), job](){
    auto guard = cancel.safeAccess();
    job();
  }, prio);
}

size_t SafePriorityThreadPoolWrapper::getNumberOfThreads() const
{
  return actualThreadPool.getNumberOfThreads();
}

void SafePriorityThreadPoolWrapper::renewCancellation()
{
  cancellation = std::make_shared<AsyncExecutionGuardAndAccessor_t<SafePriorityThreadPoolWrapper> >(std::ref(*this));
}

#include "safeprioritythreadpoolwrapper.h"

SafePriorityThreadPoolWrapper::SafePriorityThreadPoolWrapper(PriorityThreadPoolInterface &actualThreadPool_)
  : actualThreadPool(actualThreadPool_)
{

}



SimpleSafePriorityThreadPoolWrapper::SimpleSafePriorityThreadPoolWrapper(PriorityThreadPoolInterface &actualThreadPool_)
  : BaseT(actualThreadPool_)
  , cancellation(std::make_unique<AsyncExecutionCancelGuard>())
{

}



void SimpleSafePriorityThreadPoolWrapper::renewCancellation()
{
  cancellation = std::make_unique<AsyncExecutionCancelGuard>();
}

std::unique_ptr<AsyncExecutionCancelGuard> SimpleSafePriorityThreadPoolWrapper::renewCancellationAndReturnOld()
{
  std::unique_ptr<AsyncExecutionCancelGuard> result = std::move(cancellation);
  renewCancellation();
  return result;
}

ExecutionStatusToken SimpleSafePriorityThreadPoolWrapper::getCancelToken()
{
  return cancellation->getTokenPtr();
}

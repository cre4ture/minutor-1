#include "safeprioritythreadpoolwrapper.h"

SafePriorityThreadPoolWrapper::SafePriorityThreadPoolWrapper(PriorityThreadPoolInterface &actualThreadPool_)
  : actualThreadPool(actualThreadPool_)
{

}



SimpleSafePriorityThreadPoolWrapper::SimpleSafePriorityThreadPoolWrapper(PriorityThreadPoolInterface &actualThreadPool_)
  : safeWrapper(actualThreadPool_)
  , cancellation(std::make_unique<AsyncExecutionCancelGuard>())
{

}



void SimpleSafePriorityThreadPoolWrapper::renewCancellation()
{
  cancellation = std::make_unique<AsyncExecutionCancelGuard>();
}

ExecutionStatusToken SimpleSafePriorityThreadPoolWrapper::getCancelToken()
{
  return cancellation->getTokenPtr();
}

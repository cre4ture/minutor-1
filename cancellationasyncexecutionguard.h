#ifndef CANCELLATIONASYNCEXECUTIONGUARD_H
#define CANCELLATIONASYNCEXECUTIONGUARD_H

#include "cancellationmaster.h"

template<class DataT>
class AsyncExecutionCancelGuard_t: public boost::noncopyable
{
public:
  AsyncExecutionCancelGuard_t()
    : cancellation(CancellationMasterPtr_t<DataT>::create())
  {}

  ~AsyncExecutionCancelGuard_t()
  {
    cancellation.cancelAndWait();
  }

  ExecutionStatusToken_t<DataT> getTokenPtr() const
  {
    return cancellation.getTokenPtr();
  }

  void startCancellation()
  {
    cancellation.cancel();
  }

private:
  CancellationMasterPtr_t<DataT> cancellation;
};

typedef AsyncExecutionCancelGuard_t<NullData> AsyncExecutionCancelGuard;

template<class ThisT>
class CancelSafeThisAccessor_t
{
public:
  CancelSafeThisAccessor_t(ThisT& parent_, const ExecutionGuard& guard_)
    : guard(guard_)
    , myParent(parent_)
  {}

  ThisT& parent() const { return myParent; }

  ExecutionGuard token() const {
    return guard;
  }

private:
  ExecutionGuard guard;
  ThisT& myParent;
};

template<class ThisT>
class WeakCancelSafeObjectAccessor_t
{
public:
  WeakCancelSafeObjectAccessor_t(ThisT& parent_, const ExecutionStatusToken& guard_)
    : guard(guard_)
    , myParent(parent_)
  {}

  std::pair<ThisT&,ExecutionGuard> accessChecked() const
  {
    auto executionGuard = guard.createExecutionGuardChecked();

    return std::make_pair(std::ref(myParent), executionGuard);
  }

private:
  ExecutionStatusToken guard;
  ThisT& myParent;
};


#endif // CANCELLATIONASYNCEXECUTIONGUARD_H

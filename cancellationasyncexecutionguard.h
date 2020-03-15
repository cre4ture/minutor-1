#ifndef CANCELLATIONASYNCEXECUTIONGUARD_H
#define CANCELLATIONASYNCEXECUTIONGUARD_H

#include "cancellationmaster.h"

class AsyncExecutionCancelGuard: public boost::noncopyable
{
public:
  AsyncExecutionCancelGuard()
    : cancellation(CancellationMasterPtr::create())
  {}

  ~AsyncExecutionCancelGuard()
  {
    cancellation.cancelAndWait();
  }

  ExecutionStatusToken getTokenPtr() const
  {
    return cancellation.getTokenPtr();
  }

private:
  CancellationMasterPtr cancellation;
};

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

template<class ThisT>
class AsyncExecutionGuardAndObjectAccessor_t: public AsyncExecutionCancelGuard
{
public:
  AsyncExecutionGuardAndObjectAccessor_t(ThisT& parent_)
    : parent(parent_)
  {}

  CancelSafeThisAccessor_t<ThisT> getAccessor() const
  {
    return CancelSafeThisAccessor_t<ThisT>(parent, AsyncExecutionCancelGuard::getTokenPtr());
  }

  WeakCancelSafeObjectAccessor_t<ThisT> getWeakAccessor() const
  {
    return WeakCancelSafeObjectAccessor_t<ThisT>(parent, AsyncExecutionCancelGuard::getTokenPtr());
  }

private:
  ThisT& parent;
};


#endif // CANCELLATIONASYNCEXECUTIONGUARD_H

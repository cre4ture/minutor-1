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

  CancellationTokenWeakPtr getTokenPtr() const
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
  CancelSafeThisAccessor_t(ThisT& parent_, const CancellationTokenPtr& guard_)
    : guard(guard_)
    , myParent(parent_)
  {}

  ThisT& parent() const { return myParent; }

  CancellationTokenPtr token() const {
    return guard;
  }

private:
  CancellationTokenPtr guard;
  ThisT& myParent;
};

template<class ThisT>
class WeakCancelSafeObjectAccessor_t
{
public:
  WeakCancelSafeObjectAccessor_t(ThisT& parent_, const CancellationTokenWeakPtr& guard_)
    : guard(guard_)
    , myParent(parent_)
  {}

  std::pair<ThisT&,CancellationTokenPtr> accessChecked() const
  {
    auto executionGuard = guard.createExecutionGuardChecked();

    return std::make_pair(std::ref(myParent), executionGuard);
  }

private:
  CancellationTokenWeakPtr guard;
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

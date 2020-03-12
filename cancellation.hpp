#ifndef CANCELLATION_HPP
#define CANCELLATION_HPP

#include <boost/noncopyable.hpp>
#include <QSharedPointer>

#include <future>

class CancelledException: public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class CancellationTokenI
{
public:
  virtual bool isCanceled() const = 0;

  CancellationTokenI()
  {
    cancelationDoneSharedFuture = cancelationDonePromise.get_future().share();
  }

  virtual ~CancellationTokenI()
  {
    cancelationDonePromise.set_value();
  }

  auto getCancellationFuture()
  {
    return cancelationDoneSharedFuture;
  }

private:
  std::promise<void> cancelationDonePromise;
  std::shared_future<void> cancelationDoneSharedFuture;
};

class CancellationTokenPtr;

class CancellationTokenWeakPtr : public QWeakPointer<CancellationTokenI>
{
public:
  using QWeakPointer::QWeakPointer;

  std::pair<bool, QSharedPointer<CancellationTokenI> > isCanceled() const
  {
    auto strongPtr = lock();
    bool isCanceled = (!strongPtr) || strongPtr->isCanceled();
    return std::make_pair(isCanceled, strongPtr);
  }

  CancellationTokenPtr toStrongTokenPtr() const;
};

class CancellationTokenPtr : public QSharedPointer<CancellationTokenI>
{
public:
  using QSharedPointer::QSharedPointer;

  bool isCanceled() const
  {
    return isNull() || data()->isCanceled();
  }

  CancellationTokenWeakPtr toWeakToken() const
  {
    return *this;
  }
};


class Cancellation: public CancellationTokenI, public boost::noncopyable
{
public:
  Cancellation()
    : cancelled(false)
  {}

  void cancel()
  {
    cancelled = true;
  }

  virtual bool isCanceled() const override
  {
    return cancelled;
  }

private:
  volatile bool cancelled;
};

class CancellationPtr : public QSharedPointer<Cancellation>
{
public:
  using QSharedPointer::QSharedPointer;

  static CancellationPtr create()
  {
    return QSharedPointer<Cancellation>::create();
  }

  void cancelAndWait()
  {
    if (data() == nullptr)
      return;

    data()->cancel();

    auto future = data()->getCancellationFuture();
    reset();
    future.wait();
  }

  CancellationTokenPtr getToken() const
  {
    return *this;
  }

  CancellationTokenWeakPtr getWeakToken() const
  {
    return *this;
  }
};

class AsyncExecutionCancelGuard: public boost::noncopyable
{
public:
  AsyncExecutionCancelGuard()
    : cancellation(CancellationPtr::create())
  {}

  ~AsyncExecutionCancelGuard()
  {
    cancellation.cancelAndWait();
  }

  CancellationTokenPtr getToken() const
  {
    return cancellation;
  }

private:
  CancellationPtr cancellation;
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
class WeakCancelSafeThisAccessor_t
{
public:
  WeakCancelSafeThisAccessor_t(ThisT& parent_, const CancellationTokenWeakPtr& guard_)
    : guard(guard_)
    , myParent(parent_)
  {}

  std::pair<ThisT&,CancellationTokenPtr> safeAccess() const
  {
    auto cancelState = guard.isCanceled();
    if (cancelState.first)
    {
      throw CancelledException("already cancelled!");
    }

    return std::make_pair(std::ref(myParent), cancelState.second);
  }

private:
  CancellationTokenWeakPtr guard;
  ThisT& myParent;
};

template<class ThisT>
class AsyncExecutionGuardAndAccessor_t: public AsyncExecutionCancelGuard
{
public:
  AsyncExecutionGuardAndAccessor_t(ThisT& parent_)
    : parent(parent_)
  {}

  CancelSafeThisAccessor_t<ThisT> getAccessor() const
  {
    return CancelSafeThisAccessor_t<ThisT>(parent, AsyncExecutionCancelGuard::getToken());
  }

  WeakCancelSafeThisAccessor_t<ThisT> getWeakAccessor() const
  {
    return WeakCancelSafeThisAccessor_t<ThisT>(parent, AsyncExecutionCancelGuard::getToken());
  }

private:
  ThisT& parent;
};


inline CancellationTokenPtr CancellationTokenWeakPtr::toStrongTokenPtr() const
{
  return toStrongRef();
}

#endif // CANCELLATION_HPP

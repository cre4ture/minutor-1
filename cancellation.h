#ifndef CANCELLATION_HPP
#define CANCELLATION_HPP

#include <boost/noncopyable.hpp>
#include <QSharedPointer>

#include <future>
#include <atomic>

class CancelledException: public std::runtime_error
{
public:
  using std::runtime_error::runtime_error;
};

class CancellationToken: public boost::noncopyable
{
public:
  bool isCanceled() const { return cancelled; }

  CancellationToken()
    : cancelled(false)
    , cancelationDonePromise()
    , cancelationDoneSharedFuture(cancelationDonePromise.get_future().share())
  {}

  virtual ~CancellationToken()
  {
    cancelationDonePromise.set_value();
  }

  auto getCancellationFuture()
  {
    return cancelationDoneSharedFuture;
  }

protected:
  std::atomic<bool> cancelled;

private:
  std::promise<void> cancelationDonePromise;
  std::shared_future<void> cancelationDoneSharedFuture;
};

class CancellationTokenPtr;

class CancellationTokenWeakPtr : public QWeakPointer<CancellationToken>
{
public:
  using QWeakPointer::QWeakPointer;

  CancellationTokenPtr tryCreateExecutionGuard() const;

  CancellationTokenPtr createExecutionGuardChecked() const;
};

class CancellationTokenPtr : public QSharedPointer<CancellationToken>
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


inline CancellationTokenPtr CancellationTokenWeakPtr::tryCreateExecutionGuard() const
{
  CancellationTokenPtr strongPtr = lock();
  bool isCanceled = (!strongPtr) || strongPtr->isCanceled();
  return isCanceled ? CancellationTokenPtr() : strongPtr;
}

inline CancellationTokenPtr CancellationTokenWeakPtr::createExecutionGuardChecked() const
{
  CancellationTokenPtr guard = tryCreateExecutionGuard();
  if (!guard)
  {
    throw CancelledException("CancellationTokenWeakPtr::createExecutionGuardChecked(): already canceled!");
  }

  return guard;
}

#endif // CANCELLATION_HPP

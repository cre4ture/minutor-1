#ifndef CANCELLATION_HPP
#define CANCELLATION_HPP

#include <boost/noncopyable.hpp>
#include <QSharedPointer>

#include <future>

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

class CancellationTokenPtr : public QSharedPointer<CancellationTokenI>
{
public:
  using QSharedPointer::QSharedPointer;

  bool isCanceled() const
  {
    return isNull() || data()->isCanceled();
  }
};


class CancellationTokenWeakPtr : public QWeakPointer<CancellationTokenI>
{
public:
  using QWeakPointer::QWeakPointer;

  bool isCanceled() const
  {
    auto strongPtr = lock();
    return (!strongPtr) || strongPtr->isCanceled();
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


#endif // CANCELLATION_HPP

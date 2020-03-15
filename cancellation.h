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

class ExecutionStatus: public boost::noncopyable
{
public:
  bool isCanceled() const { return cancelled; }

  ExecutionStatus()
    : cancelled(false)
    , executionDonePromise()
    , executionDoneSharedFuture(executionDonePromise.get_future().share())
  {}

  virtual ~ExecutionStatus()
  {
    executionDonePromise.set_value();
  }

  auto getExecutionDoneFuture()
  {
    return executionDoneSharedFuture;
  }

protected:
  std::atomic<bool> cancelled;

private:
  std::promise<void> executionDonePromise;
  std::shared_future<void> executionDoneSharedFuture;
};

class ExecutionGuard;

class ExecutionStatusToken : public QWeakPointer<ExecutionStatus>
{
  typedef QWeakPointer<ExecutionStatus> BaseT;
public:
  ExecutionStatusToken()
  {}

  explicit ExecutionStatusToken(const ExecutionGuard& guard);

  ExecutionGuard tryCreateExecutionGuard() const;

  ExecutionGuard createExecutionGuardChecked() const;
};

class ExecutionGuard : public QSharedPointer<ExecutionStatus>
{
  typedef QSharedPointer<ExecutionStatus> BaseT;

public:
  ExecutionGuard()
    : BaseT()
  {}

  ExecutionGuard(const BaseT& other)
    : BaseT(other)
  {}

  bool isCanceled() const
  {
    return isNull() || data()->isCanceled();
  }

  ExecutionStatusToken toWeakToken() const
  {
    return ExecutionStatusToken(*this);
  }
};


inline ExecutionStatusToken::ExecutionStatusToken(const ExecutionGuard &guard)
  : BaseT(static_cast<const QSharedPointer<ExecutionStatus>&>(guard))
{}

inline ExecutionGuard ExecutionStatusToken::tryCreateExecutionGuard() const
{
  ExecutionGuard strongPtr = lock();
  bool isCanceled = (!strongPtr) || strongPtr->isCanceled();
  return isCanceled ? ExecutionGuard() : strongPtr;
}

inline ExecutionGuard ExecutionStatusToken::createExecutionGuardChecked() const
{
  ExecutionGuard guard = tryCreateExecutionGuard();
  if (!guard)
  {
    throw CancelledException("CancellationTokenWeakPtr::createExecutionGuardChecked(): already canceled!");
  }

  return guard;
}

#endif // CANCELLATION_HPP

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


// allows cancellation of asynchronous execution of jobs
// allows tracking of activity in combination with ExecutionGuard
// allows to wait until all currently connected activities are done
class ExecutionStatusBase: public boost::noncopyable
{
public:
  ExecutionStatusBase()
    : cancelled(false)
    , executionDonePromise()
    , executionDoneSharedFuture(executionDonePromise.get_future().share())
  {}

  virtual ~ExecutionStatusBase()
  {
    executionDonePromise.set_value();
  }

  bool isCanceled() const { return cancelled; }

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

struct NullData {};

template<class DataT>
class ExecutionStatus_t;

template<>
class ExecutionStatus_t<NullData>: public ExecutionStatusBase
{
public:
  using ExecutionStatusBase::ExecutionStatusBase;
};

template<class DataT>
class ExecutionStatus_t: public ExecutionStatus_t<NullData>
{
public:
  using ExecutionStatus_t<NullData>::ExecutionStatus_t;

  DataT& data() { return holdData; }

private:
  DataT holdData;
};

typedef ExecutionStatus_t<NullData> ExecutionStatus;

template<class DataT>
class ExecutionGuard_t;

// is a weak pointer to the execution status
// allows to test for cancellation
// allows to create a strong pointer which is called ExecutionGuard
template<class DataT>
class ExecutionStatusToken_t : public QWeakPointer<ExecutionStatus_t<DataT> >
{
  typedef QWeakPointer<ExecutionStatus_t<DataT> > BaseT;
public:
  ExecutionStatusToken_t()
  {}

  explicit ExecutionStatusToken_t(const ExecutionGuard_t<DataT>& guard);

  ExecutionGuard_t<DataT> tryCreateExecutionGuard() const;

  ExecutionGuard_t<DataT> createExecutionGuardChecked() const;
};

typedef ExecutionStatusToken_t<NullData> ExecutionStatusToken;

// ExecutionGuard blocks the succesfull finish of cancellation until all guards are gone
template<class DataT>
class ExecutionGuard_t : public QSharedPointer<ExecutionStatus_t<DataT> >
{
  typedef QSharedPointer<ExecutionStatus_t<DataT> > BaseT;

public:
  ExecutionGuard_t()
    : BaseT()
  {}

  ExecutionGuard_t(const BaseT& other)
    : BaseT(other)
  {}

  bool isCanceled() const
  {
    return BaseT::isNull() || BaseT::data()->isCanceled();
  }

  void checkCancellation() const
  {
    if (isCanceled())
      throw CancelledException("ExecutionGuard()::checkCancellation(): cancelled!");
  }

  ExecutionStatusToken_t<DataT> getStatusToken() const
  {
    return ExecutionStatusToken_t<DataT>(*this);
  }

  ExecutionStatusToken_t<NullData> getBaseStatusToken() const
  {
    return ExecutionStatusToken_t<NullData>(BaseT::template staticCast<ExecutionStatus_t<NullData> >());
  }
};

typedef ExecutionGuard_t<NullData> ExecutionGuard;

// --- deferred inline function implementations -----------------

template<class DataT>
inline ExecutionStatusToken_t<DataT>::ExecutionStatusToken_t(const ExecutionGuard_t<DataT> &guard)
  : BaseT(static_cast<const QSharedPointer<ExecutionStatus_t<DataT> >&>(guard))
{}

template<class DataT>
inline ExecutionGuard_t<DataT> ExecutionStatusToken_t<DataT>::tryCreateExecutionGuard() const
{
  ExecutionGuard_t<DataT> strongPtr = BaseT::lock();
  bool isCanceled = (!strongPtr) || strongPtr->isCanceled();
  return isCanceled ? ExecutionGuard_t<DataT>() : strongPtr;
}

template<class DataT>
inline ExecutionGuard_t<DataT> ExecutionStatusToken_t<DataT>::createExecutionGuardChecked() const
{
  ExecutionGuard_t<DataT> guard = tryCreateExecutionGuard();
  if (!guard)
  {
    throw CancelledException("CancellationTokenWeakPtr::createExecutionGuardChecked(): already canceled!");
  }

  return guard;
}

#endif // CANCELLATION_HPP

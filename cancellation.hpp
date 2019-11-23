#ifndef CANCELLATION_HPP
#define CANCELLATION_HPP

#include <boost/noncopyable.hpp>
#include <QSharedPointer>

class CancellationTokenI
{
public:
  virtual bool isCanceled() const = 0;
};

class CancellationTokenPtr : public QSharedPointer<CancellationTokenI>
{
public:
  using QSharedPointer::QSharedPointer;

  bool isCanceled() const
  {
    return !isNull() && data()->isCanceled();
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


#endif // CANCELLATION_HPP

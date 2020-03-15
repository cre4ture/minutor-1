#ifndef CANCELLATIONMASTER_H
#define CANCELLATIONMASTER_H

#include "cancellation.h"

class CancellationTokenMaster: public CancellationToken
{
public:
  void cancel()
  {
    cancelled = true;
  }
};

class CancellationMasterPtr : public QSharedPointer<CancellationTokenMaster>
{
public:
  using QSharedPointer::QSharedPointer;

  static CancellationMasterPtr create()
  {
    return QSharedPointer<CancellationTokenMaster>::create();
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

  CancellationTokenWeakPtr getTokenPtr() const
  {
    return *this;
  }
};

#endif // CANCELLATIONMASTER_H

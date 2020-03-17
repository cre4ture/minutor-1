#ifndef CANCELLATIONMASTER_H
#define CANCELLATIONMASTER_H

#include "executionstatus.h"

class CancellationTokenMaster: public ExecutionStatus
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

  void cancel()
  {
    if (isNull())
      return;

    data()->cancel();
  }

  void cancelAndWait()
  {
    if (isNull())
      return;

    cancel();

    auto future = data()->getExecutionDoneFuture();
    reset();
    future.wait();
  }

  ExecutionStatusToken getTokenPtr() const
  {
    return ExecutionStatusToken(this->staticCast<ExecutionStatus>());
  }
};

#endif // CANCELLATIONMASTER_H

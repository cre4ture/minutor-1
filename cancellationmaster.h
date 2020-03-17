#ifndef CANCELLATIONMASTER_H
#define CANCELLATIONMASTER_H

#include "executionstatus.h"

template<class DataT>
class CancellationTokenMaster_t: public ExecutionStatus_t<DataT>
{
  typedef ExecutionStatus_t<DataT> BaseT;
public:
  void cancel()
  {
    BaseT::cancelled = true;
  }
};

template<class DataT>
class CancellationMasterPtr_t : public QSharedPointer<CancellationTokenMaster_t<DataT> >
{
  typedef QSharedPointer<CancellationTokenMaster_t<DataT> > BaseT;

public:
  using BaseT::BaseT;

  static CancellationMasterPtr_t create()
  {
    return BaseT::create();
  }

  void cancel()
  {
    if (BaseT::isNull())
      return;

    BaseT::data()->cancel();
  }

  void cancelAndWait()
  {
    if (BaseT::isNull())
      return;

    cancel();

    auto future = BaseT::data()->getExecutionDoneFuture();
    BaseT::reset();
    future.wait();
  }

  ExecutionStatusToken_t<DataT> getTokenPtr() const
  {
    return ExecutionStatusToken_t<DataT>(BaseT::template staticCast<ExecutionStatus_t<DataT> >());
  }
};

typedef CancellationMasterPtr_t<NullData> CancellationMasterPtr;

#endif // CANCELLATIONMASTER_H

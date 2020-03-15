#include "safeinvoker.h"

#include "cancellation.h"

Q_DECLARE_METATYPE(QSharedPointer<std::function<void()>>)

SafeGuiThreadInvoker::SafeGuiThreadInvoker()
  : QObject(nullptr)
{
  QMetaTypeId<QSharedPointer<std::function<void()>>>::qt_metatype_id();
}

SafeGuiThreadInvoker::~SafeGuiThreadInvoker()
{

}

void SafeGuiThreadInvoker::invoke(std::function<void ()> functionObj)
{
  QMetaObject::invokeMethod(this, "invokeAtMainThread", Qt::AutoConnection,
                            Q_ARG(QSharedPointer<std::function<void()>>, QSharedPointer<std::function<void()>>::create(functionObj))
                            );
}

void SafeGuiThreadInvoker::invokeAtMainThread(QSharedPointer<std::function<void()>> functionObj)
{
  if (functionObj)
  {
    try
    {
      (*functionObj)();
    } catch (CancelledException e) {
      // is ignored as we use this as common way to cancel asynchronous executed functions
    }
  }
}

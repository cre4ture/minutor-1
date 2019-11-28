#include "safeinvoker.h"

Q_DECLARE_METATYPE(QSharedPointer<std::function<void()>>)

SafeInvoker::SafeInvoker()
  : QObject(nullptr)
{
  QMetaTypeId<QSharedPointer<std::function<void()>>>::qt_metatype_id();
}

SafeInvoker::~SafeInvoker()
{

}

void SafeInvoker::invoke(std::function<void ()> functionObj)
{
  QMetaObject::invokeMethod(this, "invokeAtMainThread", Qt::AutoConnection,
                            Q_ARG(QSharedPointer<std::function<void()>>, QSharedPointer<std::function<void()>>::create(functionObj))
                            );
}

void SafeInvoker::invokeAtMainThread(QSharedPointer<std::function<void()>> functionObj)
{
  if (functionObj)
    (*functionObj)();
}

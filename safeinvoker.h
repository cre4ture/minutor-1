#ifndef SAFEINVOKER_H
#define SAFEINVOKER_H

#include "executionstatus.h"

#include <QObject>
#include <QSharedPointer>

#include <functional>

class SafeGuiThreadInvoker: public QObject
{
  Q_OBJECT

public:
  SafeGuiThreadInvoker();
  ~SafeGuiThreadInvoker();

  void invoke(std::function<void()> functionObj);

  template<typename _FuncT>
  void invokeCancellable(const ExecutionStatusToken& token, const _FuncT& func)
  {
    invoke([token, func](){
      auto guard = token.createExecutionGuardChecked();
      func(guard);
    });
  }

private slots:
  void invokeAtMainThread(QSharedPointer<std::function<void()>> functionObj);
};

#endif // SAFEINVOKER_H

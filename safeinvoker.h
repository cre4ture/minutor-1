#ifndef SAFEINVOKER_H
#define SAFEINVOKER_H

#include <QObject>
#include <QSharedPointer>

#include <functional>

class SafeInvoker: public QObject
{
  Q_OBJECT

public:
  SafeInvoker();
  ~SafeInvoker();

  void invoke(std::function<void()> functionObj);

private slots:
  void invokeAtMainThread(QSharedPointer<std::function<void()>> functionObj);
};

#endif // SAFEINVOKER_H

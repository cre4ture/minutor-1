#include "threadpoolqtjob.h"

Q_DECLARE_METATYPE(ThreadPoolQtJob::ResultHandleFunctionT);

ThreadPoolQtJob::ThreadPoolQtJob(const QSharedPointer<AsyncTaskProcessorBase> &threadpool, QObject *parent)
    : QObject(parent)
    , m_threadpool(threadpool)
{
}

void ThreadPoolQtJob::enqueueJob(const ThreadPoolQtJob::JobT &job)
{
    m_threadpool->enqueueJob([this, job](){
        ResultHandleFunctionT result = job();
        QMetaObject::invokeMethod(this, result);
    });
}

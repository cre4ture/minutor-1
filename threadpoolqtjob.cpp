#include "threadpoolqtjob.h"

ThreadPoolQtJob::ThreadPoolQtJob(const QSharedPointer<AsyncTaskProcessorBase> &threadpool, QObject *parent)
    : QObject(parent)
    , m_threadpool(threadpool)
{
}

void ThreadPoolQtJob::enqueueJob(const ThreadPoolQtJob::JobT &job)
{
    m_threadpool->enqueueJob(job);
}

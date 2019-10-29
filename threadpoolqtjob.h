#ifndef THREADPOOLQTJOB_H
#define THREADPOOLQTJOB_H

#include "asynctaskprocessorbase.hpp"

#include <QObject>

class ThreadPoolQtJob : public QObject
{
    Q_OBJECT
public:
    ThreadPoolQtJob(const QSharedPointer<AsyncTaskProcessorBase>& threadpool, QObject *parent);

    typedef std::function<void()> JobT;

    void enqueueJob(const JobT& job);

private:
    QSharedPointer<AsyncTaskProcessorBase> m_threadpool;
};


#endif // THREADPOOLQTJOB_H

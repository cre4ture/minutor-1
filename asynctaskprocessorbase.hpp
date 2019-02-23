#ifndef ASYNCTASKPROCESSORBASE_HPP
#define ASYNCTASKPROCESSORBASE_HPP

#include "threadsafequeue.hpp"

#include <QSharedPointer>

class AsyncTaskProcessorBase
{
public:
    typedef std::function<void()> JobT;

    AsyncTaskProcessorBase();

    void enqueueJob(const JobT& job)
    {
        m_queue.push(job);
    }

protected:
    class ImplC;

    QSharedPointer<ImplC> m_impl;
    ThreadSafeQueue<JobT>& m_queue;
};


#endif // ASYNCTASKPROCESSORBASE_HPP

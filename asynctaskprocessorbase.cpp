#include "asynctaskprocessorbase.hpp"

#include <future>

class AsyncTaskProcessorBase::ImplC
{
public:
  ImplC(AsyncTaskProcessorBase& parent)
    : m_parent(parent)
  {
    const size_t numberOfCpuCores = std::thread::hardware_concurrency();

    for (size_t i = 0; i < numberOfCpuCores; i++)
    {
       m_futures.push_back(std::async(std::launch::async, [this]() {
        AsyncTaskProcessorBase::JobT job;
        while (m_queue.pop(job))
        {
            job();
            job = AsyncTaskProcessorBase::JobT(); // directly delete functor after execution and before blocking for wait.
        }
      }));
    }
  }

  ~ImplC()
  {
    m_queue.signalTerminate();
    for (auto& future: m_futures)
    {
      future.get();
    }
  }

  AsyncTaskProcessorBase& m_parent;
  AsyncTaskProcessorBase::QueueType m_queue;
  std::list<std::future<void> > m_futures;
};

AsyncTaskProcessorBase::AsyncTaskProcessorBase()
  : m_impl(QSharedPointer<ImplC>::create(*this))
  , m_queue(m_impl->m_queue)
{}

size_t AsyncTaskProcessorBase::getNumberOfThreads() const
{
  return m_impl->m_futures.size();
}

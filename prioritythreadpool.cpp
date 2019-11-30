#include "prioritythreadpool.h"

#include <future>

class PriorityThreadPool::HiddenImplementationC
{
public:
  HiddenImplementationC(PriorityThreadPool& parent)
    : m_parent(parent)
  {
    const size_t numberOfCpuCores = std::thread::hardware_concurrency();

    for (size_t i = 0; i < numberOfCpuCores; i++)
    {
       m_futures.push_back(std::async(std::launch::async, [this]() {
        PriorityThreadPool::JobT job;
        while (m_queue.pop(job))
        {
            job();
            job = PriorityThreadPool::JobT(); // directly delete functor after execution and before blocking for wait.
        }
      }));
    }
  }

  ~HiddenImplementationC()
  {
    m_queue.signalTerminate();
    for (auto& future: m_futures)
    {
      future.get();
    }
  }

  PriorityThreadPool& m_parent;
  PriorityThreadPool::QueueType m_queue;
  std::list<std::future<void> > m_futures;
};

PriorityThreadPool::PriorityThreadPool()
  : m_impl(QSharedPointer<HiddenImplementationC>::create(*this))
  , m_queue(m_impl->m_queue)
{}

size_t PriorityThreadPool::getNumberOfThreads() const
{
  return m_impl->m_futures.size();
}

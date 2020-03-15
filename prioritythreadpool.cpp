#include "prioritythreadpool.h"

#include "cancellation.h"

#include <future>

#include <QList>
#include <QThread>
#include <iostream>


namespace {

  class WorkerThread : public QThread
  {
  public:
    WorkerThread(std::packaged_task<void()>&& task_)
      : task(std::move(task_))
    {}

  private:
    std::packaged_task<void()> task;

    void run() override
    {
      task();
    }
  };

}

class PriorityThreadPool::HiddenImplementationC
{
public:
  HiddenImplementationC(PriorityThreadPool& parent)
    : m_parent(parent)
  {
    const size_t numberOfCpuCores = std::thread::hardware_concurrency();

    for (size_t i = 0; i < numberOfCpuCores; i++)
    {

      std::packaged_task<void()> task([this]() {
        PriorityThreadPool::JobT job;
        while (jobqueue.pop(job))
        {
          try {
            job();
          } catch (CancelledException e) {
            /* ignore */
          } catch (std::exception e) {
            std::cerr << "PriorityThreadPool(): exception caught: " << e.what() << std::endl;
          } catch (...) {
            std::cerr << "PriorityThreadPool(): unknown exception caught" << std::endl;
          }
          job = PriorityThreadPool::JobT(); // directly delete functor after execution and before blocking for wait.
        }
      });

      futures.push_back(task.get_future().share());

      auto worker = QSharedPointer<WorkerThread>::create(std::move(task));
      workers.push_back(worker);

      worker->start(QThread::Priority::LowPriority);
    }
  }

  ~HiddenImplementationC()
  {
    jobqueue.signalTerminate();

    for (auto& worker: workers)
    {
      worker->wait();
    }

    for (auto& future: futures)
    {
      future.get();
    }
  }

  PriorityThreadPool& m_parent;
  PriorityThreadPool::QueueType jobqueue;
  QList<QSharedPointer<WorkerThread> > workers;
  QList<std::shared_future<void> > futures;
};

PriorityThreadPool::PriorityThreadPool()
  : m_impl(QSharedPointer<HiddenImplementationC>::create(*this))
  , m_queue(m_impl->jobqueue)
{}

size_t PriorityThreadPool::getNumberOfThreads() const
{
  return m_impl->futures.size();
}

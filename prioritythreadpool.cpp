#include "prioritythreadpool.h"

#include "executionstatus.h"

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

class PriorityThreadPoolN::HiddenImplementationC
{
public:
  HiddenImplementationC(PriorityThreadPoolN& parent, size_t threadCount)
    : m_parent(parent)
  {
    for (size_t i = 0; i < threadCount; i++)
    {

      std::packaged_task<void()> task([this]() {
        PriorityThreadPoolN::JobT job;
        while (jobqueue.pop(job))
        {
          try {
            job();
          } catch (CancelledException e) {
            /* ignore */
          } catch (std::exception e) {
            std::cerr << "PriorityThreadPoolN(): exception caught: " << e.what() << std::endl;
          } catch (...) {
            std::cerr << "PriorityThreadPoolN(): unknown exception caught" << std::endl;
          }
          job = PriorityThreadPoolN::JobT(); // directly delete functor after execution and before blocking for wait.
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

  PriorityThreadPoolN& m_parent;
  PriorityThreadPoolN::QueueType jobqueue;
  QList<QSharedPointer<WorkerThread> > workers;
  QList<std::shared_future<void> > futures;
};

PriorityThreadPoolN::PriorityThreadPoolN(size_t threadCount)
  : m_impl(QSharedPointer<HiddenImplementationC>::create(*this, threadCount))
  , m_queue(m_impl->jobqueue)
{}

size_t PriorityThreadPoolN::getNumberOfThreads() const
{
  return m_impl->futures.size();
}

PriorityThreadPool::PriorityThreadPool()
  : PriorityThreadPoolN(std::thread::hardware_concurrency())
{

}

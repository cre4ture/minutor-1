#include "searchchunkswidget.h"
#include "ui_searchchunkswidget.h"

#include "./chunkcache.h"
#include "./chunkmath.hpp"
#include "./prioritythreadpool.h"
#include "./range.h"

#include <QVariant>
#include <QTreeWidgetItem>
#include <boost/noncopyable.hpp>
#include <QtConcurrent/QtConcurrent>

SearchChunksWidget::SearchChunksWidget(const SearchEntityWidgetInputC& input)
  : QWidget(input.parent)
  , ui(new Ui::SearchChunksWidget)
  , m_input(input)
  , m_searchRunning(false)
  , m_invoker()
{
  ui->setupUi(this);

  auto layout = new QHBoxLayout(ui->plugin_context);
  ui->plugin_context->setLayout(layout);
  layout->setSizeConstraint(QLayout::SizeConstraint::SetMinimumSize);
  layout->addWidget(&m_input.searchPlugin->getWidget());

  qRegisterMetaType<QSharedPointer<SearchPluginI::ResultListT> >("QSharedPointer<SearchPluginI::ResultListT>");
}

SearchChunksWidget::~SearchChunksWidget()
{
  cancelSearch();
}

static Range<float> helperRangeCreation(const QCheckBox& checkBox, const QSpinBox& sb1, const QSpinBox& sb2)
{
  if (!checkBox.isChecked())
  {
    return Range<float>::max();
  }
  else
  {
    return Range<float>::createFromUnorderedParams(sb1.value(), sb2.value());
  }
}


template<class JobQueueT, class JobT, class ExecutionGuardT>
class JobPacker
{
public:
  JobPacker(JobQueueT& queue_)
    : queue(queue_)
  {}

  void enqueueJob(const JobT& job)
  {
    pack.push_back(job);
    if (pack.size() > 50)
    {
      queue.enqueueJob([joblist = std::move(pack)](const ExecutionGuardT& guard)
      {
        for (auto& job: joblist)
        {
          guard.checkCancellation();
          job(guard);
        }
      }, JobPrio::low);
    }
  }

private:
  JobQueueT& queue;
  std::vector<JobT> pack;
};


void SearchChunksWidget::on_pb_search_clicked()
{
  if (!currentfuture.isCanceled())
  {
    cancelSearch();
    return;
  }

  const Range<float> range_y = helperRangeCreation(*ui->check_range_y, *ui->sb_y_start, *ui->sb_y_end);
  currentSearch = QSharedPointer<AsyncSearch>::create(*this, range_y, m_input.searchPlugin, m_input.cache);

  ui->pb_search->setText("Cancel");

  ui->resultList->clearResults();
  ui->resultList->setPointOfInterest(m_input.posOfInterestProvider());

  const int radius = 1 + (ui->sb_radius->value() / CHUNK_SIZE);
  ui->progressBar->reset();
  ui->progressBar->setMaximum(radius * radius * 4);

  const bool successfull_init = m_input.searchPlugin->initSearch();
  if (!successfull_init)
  {
    return;
  }

  const QVector2D poi = getChunkCoordinates(m_input.posOfInterestProvider());
  const QVector2D radius2d(radius, radius);

  QRect searchRange((poi - radius2d).toPoint(), (poi + radius2d).toPoint());

  //JobPacker<ConvenientJobCancellingAsyncCallWrapper_t, std::function<void(const MyExecutionGuard& guard)>, MyExecutionGuard> packer(safeThreadPoolI);

  auto chunks = QSharedPointer<QList<ChunkID> >::create();

  for (RectangleInnerToOuterIterator it(searchRange); it != it.end(); ++it)
  {
    const ChunkID id(it->getX(), it->getZ());
    chunks->append(id);
  }

  currentfuture = QtConcurrent::map(*chunks, [currentSearch = currentSearch, chunks /* needed to keep list alive during search */](const ChunkID& id){
    auto master = CancellationMasterPtr_t<QSharedPointer<AsyncSearch> >::create();
    currentSearch->loadAndSearchChunk_async(id, master.getTokenPtr().createExecutionGuardChecked());
  });
}

void SearchChunksWidget::AsyncSearch::loadAndSearchChunk_async(ChunkID id, const MyExecutionGuard &guard)
{
  auto chunk = cache->getChunkSynchronously(id);

  guard.checkCancellation();

  searchLoadedChunk_async(chunk, guard);
}

void SearchChunksWidget::AsyncSearch::searchLoadedChunk_async(const QSharedPointer<Chunk>& chunk, const MyExecutionGuard &guard)
{
  QSharedPointer<SearchPluginI::ResultListT> results;

  if (chunk)
  {
    results = searchExistingChunk_async(chunk, guard);
  }

  parent.m_invoker.invokeCancellable(guard.getBaseStatusToken(),[&parent = parent, results](const ExecutionGuard& guard){
    parent.displayResults(results, ChunkID());
  });
}

QSharedPointer<SearchPluginI::ResultListT> SearchChunksWidget::AsyncSearch::searchExistingChunk_async(const QSharedPointer<Chunk>& chunk, const MyExecutionGuard &guard)
{
  ChunkID id(chunk->getChunkX(), chunk->getChunkZ());

  auto strong = searchPlugin.lock();
  if (!strong)
  {
    return QSharedPointer<SearchPluginI::ResultListT>();
  }

  auto results_tmp = strong->searchChunk(*chunk);
  auto results = QSharedPointer<SearchPluginI::ResultListT>::create();

  guard.checkCancellation();

  for (const auto& result: results_tmp)
  {
    if (range_y.isInsideRange(result.pos.y()))
    {
      results->push_back(result);
    }
  }

  return results;
}


void SearchChunksWidget::displayResults(QSharedPointer<SearchPluginI::ResultListT> results,
                                        ChunkID id)
{
  if (results)
  {
    for (const auto& result: *results)
    {
      ui->resultList->addResult(result);
    }
  }

  addOneToProgress(id);
}

void SearchChunksWidget::addOneToProgress(ChunkID /* id */)
{
  ui->progressBar->setValue(ui->progressBar->value() + 1);

  if (ui->progressBar->maximum() == ui->progressBar->value())
  {
    m_invoker.invoke([this](){cancelSearch();}); // invoke instead of calling directly to prevent a deadlock when called from a guarded workpackage
  }
}

void SearchChunksWidget::cancelSearch()
{
  currentfuture.cancel();
  currentfuture.waitForFinished();

  //safeThreadPoolI.renewCancellation();

  ui->pb_search->setText("Search");

  ui->resultList->searchDone();

  currentSearch.reset();
}

void SearchChunksWidget::on_resultList_jumpTo(const QVector3D &pos)
{
  emit jumpTo(pos);
}

void SearchChunksWidget::on_resultList_highlightEntities(QVector<QSharedPointer<OverlayItem> > item)
{
  emit highlightEntities(item);
}

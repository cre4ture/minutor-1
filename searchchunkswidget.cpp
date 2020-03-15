#include "searchchunkswidget.h"
#include "ui_searchchunkswidget.h"

#include "./chunkcache.h"
#include "./chunkmath.hpp"
#include "./prioritythreadpool.h"
#include "./range.h"

#include <QVariant>
#include <QTreeWidgetItem>
#include <boost/noncopyable.hpp>

SearchChunksWidget::SearchChunksWidget(const SearchEntityWidgetInputC& input)
  : QWidget(input.parent)
  , ui(new Ui::SearchChunksWidget)
  , m_input(input)
  , m_invoker()
  , safeThreadPoolI(*m_input.threadpool)
  , m_searchRunning(false)
{
  ui->setupUi(this);

  connect(m_input.cache.data(), SIGNAL(chunkLoaded(const QSharedPointer<Chunk>&,int,int)),
          this, SLOT(chunkLoaded(const QSharedPointer<Chunk>&,int,int)));

  auto layout = new QHBoxLayout(ui->plugin_context);
  ui->plugin_context->setLayout(layout);
  layout->setSizeConstraint(QLayout::SizeConstraint::SetMinimumSize);
  layout->addWidget(&m_input.searchPlugin->getWidget());

  qRegisterMetaType<QSharedPointer<SearchPluginI::ResultListT> >("QSharedPointer<SearchPluginI::ResultListT>");
}

SearchChunksWidget::~SearchChunksWidget()
{
}

void SearchChunksWidget::on_pb_search_clicked()
{
  if (currentToken.tryCreateExecutionGuard())
  {
    cancelSearch();
    return;
  }

  currentToken = safeThreadPoolI.getCancelToken();
  auto weakToken = currentToken;

  ui->pb_search->setText("Cancel");

  m_chunksRequestedToSearchList.clear();

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

  size_t count = 0;
  for (RectangleInnerToOuterIterator it(searchRange); it != it.end(); ++it)
  {
    const ChunkID id(it->getX(), it->getZ());

    requestSearchingOfChunk(id);

    if ((count++ % 100) == 0)
      QApplication::processEvents();

    if (!weakToken.tryCreateExecutionGuard())
    {
      return;
    }
  }
}

void SearchChunksWidget::requestSearchingOfChunk(ChunkID id)
{
  m_chunksRequestedToSearchList[id] = true;

  {
    ChunkCache::Locker locked_cache(*m_input.cache);

    QSharedPointer<Chunk> chunk;
    if (locked_cache.isCached(id, chunk)) // can return true and nullptr in case of inexistend chunk
    {
      chunkLoaded(chunk, id.getX(), id.getZ());
      return;
    }
  }

  safeThreadPoolI.enqueueJob([this, id](const CancellationTokenPtr& guard)
  {
    auto chunk = m_input.cache->getChunkSynchronously(id);

    m_invoker.invokeCancellable(guard, [this, chunk, id](const CancellationTokenPtr& guard){
      chunkLoaded(chunk, id.getX(), id.getZ());
    });
  });
}

void SearchChunksWidget::chunkLoaded(const QSharedPointer<Chunk>& chunk, int x, int z)
{
  auto guard = currentToken.tryCreateExecutionGuard();
  if (!guard)
  {
    return;
  }

  const ChunkID id(x, z);

  bool& isChunkToSearch = m_chunksRequestedToSearchList[id];
  if (!isChunkToSearch)
  {
    return; // already searched or not of interest
  }

  isChunkToSearch = false; // mark as done

  if (chunk)
  {
    searchLoadedChunk(chunk);
  }
  else
  {
    addOneToProgress(id);
  }
}

Range<float> helperRangeCreation(const QCheckBox& checkBox, const QSpinBox& sb1, const QSpinBox& sb2)
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

void SearchChunksWidget::searchLoadedChunk(const QSharedPointer<Chunk>& chunk)
{
  const Range<float> range_y = helperRangeCreation(*ui->check_range_y, *ui->sb_y_start, *ui->sb_y_end);

  auto job = [this, chunk, range_y, searchPlug = m_input.searchPlugin.toWeakRef()](const CancellationTokenPtr& guard)
  {
    ChunkID id(chunk->getChunkX(), chunk->getChunkZ());

    auto strong = searchPlug.lock();
    if (!strong)
    {
      return;
    }

    auto results_tmp = strong->searchChunk(*chunk);
    auto results = QSharedPointer<SearchPluginI::ResultListT>::create();

    for (const auto& result: results_tmp)
    {
      if (range_y.isInsideRange(result.pos.y()))
      {
        results->push_back(result);
      }
    }

    m_invoker.invokeCancellable(guard, [this, results, id](const CancellationTokenPtr& guard){
      displayResults(results, id);
    });
  };

  safeThreadPoolI.enqueueJob(job, JobPrio::high);
}


void SearchChunksWidget::displayResults(QSharedPointer<SearchPluginI::ResultListT> results,
                                        ChunkID id)
{
  for (const auto& result: *results)
  {
    ui->resultList->addResult(result);
  }

  addOneToProgress(id);
}

void SearchChunksWidget::addOneToProgress(ChunkID /* id */)
{
  ui->progressBar->setValue(ui->progressBar->value() + 1);

  if (ui->progressBar->maximum() == ui->progressBar->value())
  {
    cancelSearch();
  }
}

void SearchChunksWidget::cancelSearch()
{
  safeThreadPoolI.renewCancellation();

  ui->pb_search->setText("Search");

  ui->resultList->searchDone();
}

void SearchChunksWidget::on_resultList_jumpTo(const QVector3D &pos)
{
  emit jumpTo(pos);
}

void SearchChunksWidget::on_resultList_highlightEntities(QVector<QSharedPointer<OverlayItem> > item)
{
  emit highlightEntities(item);
}

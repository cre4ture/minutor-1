#include "searchchunkswidget.h"
#include "ui_searchchunkswidget.h"

#include "./chunkcache.h"
#include "./chunkmath.hpp"
#include "./asynctaskprocessorbase.hpp"
#include "./range.h"

#include <QVariant>
#include <QTreeWidgetItem>
#include <boost/noncopyable.hpp>

SearchChunksWidget::SearchChunksWidget(const SearchEntityWidgetInputC& input)
  : QWidget(input.parent)
    , ui(new Ui::SearchChunksWidget)
    , m_input(input)
    , m_invoker()
    , m_threadPool(m_input.threadpool)
    , m_searchRunning(false)
    , cancellation()
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
  cancelSearch();

  ui->plugin_context->layout()->removeWidget(&m_input.searchPlugin->getWidget());

  delete ui;
}

void SearchChunksWidget::on_pb_search_clicked()
{
  if (cancellation)
  {
    cancelSearch();
    return;
  }

  ui->pb_search->setText("Cancel");

  cancellation = CancellationPtr::create();

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
  for (RectangleIterator it(searchRange); it != it.end(); ++it)
  {
    const ChunkID id(it.getX(), it.getZ());

    requestSearchingOfChunk(id);

    if (cancellation->isCanceled())
    {
      return;
    }

    if ((count++ % 100) == 0)
      QApplication::processEvents();
  }
}

void SearchChunksWidget::requestSearchingOfChunk(ChunkID id)
{
  m_chunksRequestedToSearchList[id] = true;

  ChunkCache::Locker locked_cache(*m_input.cache);

  QSharedPointer<Chunk> chunk;
  if (locked_cache.isCached(id, chunk)) // can return true and nullptr in case of inexistend chunk
  {
    chunkLoaded(chunk, id.getX(), id.getZ());
    return;
  }

  m_threadPool->enqueueJob([this, id, cancelToken = cancellation.getToken()]()
  {
    if (!cancelToken.isCanceled())
    {
      auto chunk = m_input.cache->getChunkSynchronously(id);

      m_invoker.invoke([this, chunk, id](){
        chunkLoaded(chunk, id.getX(), id.getZ());
      });
    }
  });
}

void SearchChunksWidget::chunkLoaded(const QSharedPointer<Chunk>& chunk, int x, int z)
{
  if (!cancellation || cancellation->isCanceled())
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

  auto job = [this, chunk, range_y, cancelToken = cancellation.getToken(), searchPlug = m_input.searchPlugin]()
  {
    if (cancelToken.isCanceled())
    {
      return;
    }

    ChunkID id(chunk->getChunkX(), chunk->getChunkZ());

    auto results_tmp = searchPlug->searchChunk(*chunk);
    auto results = QSharedPointer<SearchPluginI::ResultListT>::create();

    for (const auto& result: results_tmp)
    {
      if (range_y.isInsideRange(result.pos.y()))
      {
        results->push_back(result);
      }
    }

    m_invoker.invoke([this, cancelToken = cancellation.getToken(), results, id](){
      if (cancelToken.isCanceled())
      {
        return;
      }

      displayResults(results, id);
    });
  };

  m_threadPool->enqueueJob(job, AsyncTaskProcessorBase::JobPrio::high);
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
  cancellation.cancelAndWait();

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

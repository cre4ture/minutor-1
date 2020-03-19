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
  , m_searchRunning(false)
  , m_invoker()
  , safeThreadPoolI(*m_input.threadpool)
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

void SearchChunksWidget::on_pb_search_clicked()
{
  if (currentSearch)
  {
    cancelSearch();
    return;
  }

  const Range<float> range_y = helperRangeCreation(*ui->check_range_y, *ui->sb_y_start, *ui->sb_y_end);
  currentSearch = QSharedPointer<AsyncSearch>::create(*this, range_y, m_input.searchPlugin, m_input.cache);

  auto weakToken = safeThreadPoolI.getCancelToken();
  weakToken.createExecutionGuardChecked()->data() = currentSearch;

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
  safeThreadPoolI.enqueueJob([id](const MyExecutionGuard& guard)
  {
    guard->data()->loadAndSearchChunk_async(id, guard);
  }, JobPrio::low);
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
  safeThreadPoolI.renewCancellation();

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

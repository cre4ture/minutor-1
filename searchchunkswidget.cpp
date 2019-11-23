#include "searchchunkswidget.h"
#include "ui_searchchunkswidget.h"

#include "./chunkcache.h"
#include "chunkmath.hpp"
#include "asynctaskprocessorbase.hpp"

#include <QVariant>
#include <QTreeWidgetItem>
#include <boost/noncopyable.hpp>

SearchChunksWidget::SearchChunksWidget(const SearchEntityWidgetInputC& input)
  : QWidget(input.parent)
  , ui(new Ui::SearchChunksWidget)
  , m_input(input)
  , m_threadPool(m_input.threadpool)
  , m_searchRunning(false)
  , cancellation()
{
    ui->setupUi(this);

    connect(m_input.cache.data(), SIGNAL(chunkLoaded(const QSharedPointer<Chunk>&,int,int)),
            this, SLOT(chunkLoaded(const QSharedPointer<Chunk>&,int,int)));

    ui->plugin_context->setLayout(new QHBoxLayout(ui->plugin_context));
    ui->plugin_context->layout()->addWidget(&m_input.searchPlugin->getWidget());

    qRegisterMetaType<QSharedPointer<SearchPluginI::ResultListT> >("QSharedPointer<SearchPluginI::ResultListT>");
}

SearchChunksWidget::~SearchChunksWidget()
{
    ui->plugin_context->layout()->removeWidget(&m_input.searchPlugin->getWidget());
    delete ui;
}

void SearchChunksWidget::setVillageLocations(const QList<QSharedPointer<OverlayItem> > &villages)
{
    m_villages = villages;
}

void SearchChunksWidget::on_pb_search_clicked()
{
  class SearchStateResetGuard: public boost::noncopyable
  {
  public:
    SearchStateResetGuard(SearchChunksWidget& parent)
      : m_parent(parent)
    {
      m_parent.m_searchRunning = true;
      m_parent.ui->pb_search->setText("Cancel");
    }

    ~SearchStateResetGuard()
    {
      m_parent.m_searchRunning = false;
    }

  private:
    SearchChunksWidget& m_parent;
  };

  if (cancellation)
  {
    cancellation->cancel();
    cancellation.reset();
    ui->pb_search->setText("Search");
    return;
  }

  SearchStateResetGuard searchRunningGuard(*this);
  auto myCancellation = QSharedPointer<Cancellation>::create();
  cancellation = myCancellation;

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

  for (RectangleIterator it(searchRange); it != it.end(); ++it)
  {
    const ChunkID id(it.getX(), it.getZ());

    const bool searchNeeded = checkLocationIfSearchIsNeeded(id);
    if (searchNeeded)
    {
      requestSearchingOfChunk(id);
    }
    else
    {
      addOneToProgress(id);
    }

    if (myCancellation->isCanceled())
    {
        return;
    }

    QApplication::processEvents();
  }
}

bool SearchChunksWidget::checkLocationIfSearchIsNeeded(ChunkID id)
{
  bool searchThisChunk = true;
  if (ui->check_villages->isChecked())
  {
      searchThisChunk = searchThisChunk && villageFilter(id);
  }

  return searchThisChunk;
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

  CancellationTokenPtr myCancellation = cancellation;
  m_threadPool->enqueueJob([this, id, myCancellation](){
    if (!myCancellation.isCanceled())
    {
      auto chunk = m_input.cache->getChunkSync(id);
      QMetaObject::invokeMethod(this, "chunkLoaded",
                                Q_ARG(QSharedPointer<Chunk>, chunk),
                                Q_ARG(int, id.getX()),
                                Q_ARG(int, id.getZ())
                                );
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

template <typename _ValueT>
class Range
{
public:
    Range(_ValueT start_including, _ValueT end_including)
        : start(start_including)
        , end(end_including)
    {}

    static Range createFromUnorderedParams(_ValueT v1, _ValueT v2)
    {
        if (v1 > v2)
        {
            std::swap(v1, v2);
        }

        return Range(v1, v2);
    }

    static Range max()
    {
        return Range(std::numeric_limits<_ValueT>::lowest(), std::numeric_limits<_ValueT>::max());
    }

    const _ValueT start;
    const _ValueT end;

    bool isInsideRange(_ValueT value) const
    {
        return (value >= start) && (value <= end);
    }
};

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
    const Range<float> range_x = helperRangeCreation(*ui->check_range_x, *ui->sb_x_start, *ui->sb_x_end);
    const Range<float> range_y = helperRangeCreation(*ui->check_range_y, *ui->sb_y_start, *ui->sb_y_end);
    const Range<float> range_z = helperRangeCreation(*ui->check_range_z, *ui->sb_z_start, *ui->sb_z_end);

    CancellationTokenPtr myCancellation = cancellation;
    auto job = [this, chunk, range_x, range_y, range_z, myCancellation, searchPlug = m_input.searchPlugin]()
    {
      if (myCancellation.isCanceled())
      {
        return;
      }

      ChunkID id(chunk->getChunkX(), chunk->getChunkZ());

      auto results_tmp = searchPlug->searchChunk(*chunk);
      auto results = QSharedPointer<SearchPluginI::ResultListT>::create();

      for (const auto& result: results_tmp)
      {
          if (range_x.isInsideRange(result.pos.x()) &&
              range_y.isInsideRange(result.pos.y()) &&
              range_z.isInsideRange(result.pos.z()))
          {
              results->push_back(result);
          }
      }

      QMetaObject::invokeMethod(this, "displayResults",
                                Q_ARG(QSharedPointer<SearchPluginI::ResultListT>, results),
                                Q_ARG(ChunkID, id)
                                );
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

bool SearchChunksWidget::villageFilter(ChunkID id) const
{
    const float limit_squared = ui->sb_villages_radius->value() * ui->sb_villages_radius->value();
    const auto location = id.centerCoordinates();
    for (auto& village: m_villages)
    {
        QVector2D vector(location.getX() - village->midpoint().x,
                         location.getZ() - village->midpoint().z);
        if (vector.lengthSquared() < limit_squared)
        {
            return true;
        }
    }

    return false;
}

void SearchChunksWidget::addOneToProgress(ChunkID /* id */)
{
  ui->progressBar->setValue(ui->progressBar->value() + 1);

  if (ui->progressBar->maximum() == ui->progressBar->value())
  {
    ui->resultList->searchDone();
    cancellation.reset();
    ui->pb_search->setText("Search");
  }
}

void SearchChunksWidget::on_resultList_jumpTo(const QVector3D &pos)
{
    emit jumpTo(pos);
}

void SearchChunksWidget::on_resultList_highlightEntities(QVector<QSharedPointer<OverlayItem> > item)
{
    emit highlightEntities(item);
}

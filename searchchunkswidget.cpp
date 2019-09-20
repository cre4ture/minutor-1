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
  , m_threadPoolWrapper(m_input.threadpool, this)
  , m_searchRunning(false)
  , m_requestCancel(false)
{
    ui->setupUi(this);

    connect(m_input.cache.get(), SIGNAL(chunkLoaded(const QSharedPointer<Chunk>&,int,int)),
            this, SLOT(chunkLoaded(const QSharedPointer<Chunk>&,int,int)));

    ui->plugin_context->setLayout(new QHBoxLayout(ui->plugin_context));
    ui->plugin_context->layout()->addWidget(&m_input.searchPlugin->getWidget());
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
            m_parent.ui->pb_search->setText("Search");
        }

    private:
        SearchChunksWidget& m_parent;
    };

    if (m_searchRunning)
    {
        m_requestCancel = true;
        ui->pb_search->setText("Cancelling ...");
        return;
    }

    SearchStateResetGuard searchRunningGuard(*this);
    m_requestCancel = false;

    m_chunksToSearchList.clear();

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

    for (int z= -radius; z <= radius; z++)
    {
        for (int x = -radius; x <= radius; x++)
        {
            checkLocationAndTrySearchChunk(ChunkID(poi.x() + x, poi.y() + z));

            if (m_requestCancel)
            {
                return;
            }
        }
        QApplication::processEvents();
    }
}

void SearchChunksWidget::chunkLoaded(const QSharedPointer<Chunk>& chunk, int x, int z)
{
    if (m_requestCancel)
    {
        return;
    }

    if (chunk)
    {
        searchLoadedChunk(chunk);
    }
    else
    {
        oneChunkDoneNotify(ChunkID(x, z));
    }
}

void SearchChunksWidget::checkLocationAndTrySearchChunk(ChunkID id)
{
    m_chunksToSearchDoneList.insert(id);

    bool searchThisChunk = true;
    if (ui->check_villages->isChecked())
    {
        searchThisChunk = searchThisChunk && villageFilter(id);
    }

    if (!searchThisChunk)
    {
        oneChunkDoneNotify(id);
        return;
    }

    trySearchChunk(id);
}

void SearchChunksWidget::trySearchChunk(ChunkID id)
{
    m_chunksToSearchList.insert(id);

    QSharedPointer<Chunk> chunk = nullptr;
    ChunkCache::Locker locked_cache(*m_input.cache);

    if (locked_cache.isCached(id, chunk)) // can return true and nullptr in case of inexistend chunk
    {
        chunkLoaded(chunk, id.getX(), id.getZ());
        return;
    }

    if (locked_cache.fetch(chunk, id)) // normally schedules async loading and return false. But it can happen in rare cases that chunk has been loaded since isCached() check returned
    {
        chunkLoaded(chunk, id.getX(), id.getZ());
        return;
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
    ChunkID id(chunk->getChunkX(), chunk->getChunkZ());

    auto it = m_chunksToSearchList.find(id);
    if (it == m_chunksToSearchList.end())
    {
        return; // already searched or not of interest
    }

    m_chunksToSearchList.erase(it);

    const Range<float> range_x = helperRangeCreation(*ui->check_range_x, *ui->sb_x_start, *ui->sb_x_end);
    const Range<float> range_y = helperRangeCreation(*ui->check_range_y, *ui->sb_y_start, *ui->sb_y_end);
    const Range<float> range_z = helperRangeCreation(*ui->check_range_z, *ui->sb_z_start, *ui->sb_z_end);

    m_threadPoolWrapper.enqueueJob([this, id, chunk, range_x, range_y, range_z]()
    {
        auto results_tmp = m_input.searchPlugin->searchChunk(*chunk);
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

        return std::function<void()>([this, id, results]()
        {
            for (const auto& result: *results)
            {
                ui->resultList->addResult(result);
            }

            oneChunkDoneNotify(id);
        });
    });
}

bool SearchChunksWidget::villageFilter(ChunkID id) const
{
    const float limit_squared = ui->sb_villages_radius->value() * ui->sb_villages_radius->value();
    const auto location = id.toCoordinates();
    for (auto& village: m_villages)
    {
        QVector2D vector(location.x - village->midpoint().x,
                         location.z - village->midpoint().z);
        if (vector.lengthSquared() < limit_squared)
        {
            return true;
        }
    }

    return false;
}

void SearchChunksWidget::oneChunkDoneNotify(ChunkID id)
{
  ui->progressBar->setValue(ui->progressBar->value() + 1);

  const size_t erased = m_chunksToSearchDoneList.erase(id);
  if (!m_searchRunning && (erased == 1) && (m_chunksToSearchDoneList.empty()))
  {
    ui->resultList->searchDone();
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

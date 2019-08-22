#include "searchchunkswidget.h"
#include "ui_searchentitywidget.h"

#include "./chunkcache.h"
#include "chunkmath.hpp"
#include "asynctaskprocessorbase.hpp"

#include <QVariant>
#include <QTreeWidgetItem>
#include <boost/noncopyable.hpp>

SearchChunksWidget::SearchChunksWidget(const SearchEntityWidgetInputC& input)
  : QWidget(input.parent)
  , ui(new Ui::SearchEntityWidget)
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

    m_searchedBlockCoordinates.clear();

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
            trySearchChunk(poi.x() + x, poi.y() + z);

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
    if (m_requestCancel || !m_searchRunning)
    {
        return;
    }

    if (chunk)
    {
        searchChunk(chunk);
    }
    else
    {
        ui->progressBar->setValue(ui->progressBar->value() + 1);
    }
}

void SearchChunksWidget::trySearchChunk(int x, int z)
{
    QSharedPointer<Chunk> chunk = nullptr;
    ChunkCache::Locker locked_cache(*m_input.cache);

    if (locked_cache.isCached(x, z, chunk)) // can return true and nullptr in case of inexistend chunk
    {
        chunkLoaded(chunk, x, z);
        return;
    }

    if (locked_cache.fetch(chunk, x, z)) // normally schedules async loading and return false. But it can happen in rare cases that chunk has been loaded since isCached() check returned
    {
        chunkLoaded(chunk, x, z);
        return;
    }
}

void SearchChunksWidget::searchChunk(const QSharedPointer<Chunk>& chunk)
{
    ChunkID id(chunk->getChunkX(), chunk->getChunkZ());

    auto it = m_searchedBlockCoordinates.find(id);
    if (it != m_searchedBlockCoordinates.end())
    {
        return; // already searched!
    }

    m_searchedBlockCoordinates.insert(id);

    m_threadPoolWrapper.enqueueJob([this, chunk]()
    {
        auto results = QSharedPointer<SearchPluginI::ResultListT>::create(m_input.searchPlugin->searchChunk(*chunk));

        return std::function<void()>([this, results]()
        {
            for (const auto& result: *results)
            {
                ui->resultList->addResult(result);
            }

            ui->progressBar->setValue(ui->progressBar->value() + 1);
        });
    });
}

void SearchChunksWidget::on_resultList_jumpTo(const QVector3D &pos)
{
    emit jumpTo(pos);
}

void SearchChunksWidget::on_resultList_highlightEntities(QVector<QSharedPointer<OverlayItem> > item)
{
    emit highlightEntities(item);
}

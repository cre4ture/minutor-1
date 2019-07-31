#include "searchentitywidget.h"
#include "ui_searchentitywidget.h"

#include "./chunkcache.h"
#include "chunkmath.hpp"

#include <QVariant>
#include <QTreeWidgetItem>

SearchEntityWidget::SearchEntityWidget(const SearchEntityWidgetInputC& input)
  : QWidget(input.parent)
  , ui(new Ui::SearchEntityWidget)
  , m_input(input)
{
    ui->setupUi(this);

    connect(m_input.cache.get(), SIGNAL(chunkLoaded(const QSharedPointer<Chunk>&,int,int)),
            this, SLOT(chunkLoaded(const QSharedPointer<Chunk>&,int,int)));

    ui->plugin_context->setLayout(new QHBoxLayout(ui->plugin_context));
    ui->plugin_context->layout()->addWidget(&m_input.searchPlugin->getWidget());
}

SearchEntityWidget::~SearchEntityWidget()
{
    ui->plugin_context->layout()->removeWidget(&m_input.searchPlugin->getWidget());
    delete ui;
}

void SearchEntityWidget::on_pb_search_clicked()
{
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
        }
    }
}

void SearchEntityWidget::chunkLoaded(const QSharedPointer<Chunk>& chunk, int x, int z)
{
    if (chunk)
    {
        searchChunk(*chunk);
    }
    else
    {
        ui->progressBar->setValue(ui->progressBar->value() + 1);
    }
}

void SearchEntityWidget::trySearchChunk(int x, int z)
{
    QSharedPointer<Chunk> chunk = nullptr;
    ChunkCache::Locker locked_cache(*m_input.cache);

    if (locked_cache.isLoaded(x, z, chunk))
    {
        searchChunk(*chunk);
        return;
    }

    locked_cache.fetch(chunk, x, z);
    if (chunk != nullptr)
    {
        searchChunk(*chunk);
    }
}

void SearchEntityWidget::searchChunk(Chunk& chunk)
{
    ChunkID id(chunk.getChunkX(), chunk.getChunkZ());

    auto it = m_searchedBlockCoordinates.find(id);
    if (it != m_searchedBlockCoordinates.end())
    {
        return; // already searched!
    }

    m_searchedBlockCoordinates.insert(id);

    m_input.searchPlugin->searchChunk(*ui->resultList, chunk);

    ui->progressBar->setValue(ui->progressBar->value() + 1);
}

void SearchEntityWidget::on_resultList_jumpTo(const QVector3D &pos)
{
    emit jumpTo(pos);
}

void SearchEntityWidget::on_resultList_highlightEntity(QSharedPointer<OverlayItem> item)
{
    emit highlightEntity(item);
}

#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include "propertietreecreator.h"
#include "overlayitem.h"
#include "entityevaluator.h"
#include "searchplugininterface.h"
#include "chunkcachetypes.h"
#include "asynctaskprocessorbase.hpp"
#include "threadpoolqtjob.h"

#include <QWidget>
#include <set>

namespace Ui {
class SearchChunksWidget;
}

class ChunkCache;
class Chunk;
class SearchResultWidget;
class GenericIdentifier;
class AsyncTaskProcessorBase;

struct SearchEntityWidgetInputC
{
    typedef std::function<QVector3D()> PositionProviderT;

    SearchEntityWidgetInputC(const QSharedPointer<AsyncTaskProcessorBase>& threadpool_,
                             const QSharedPointer<ChunkCache>& cache_,
//                             EntityDefitionsConfig definitions_,
                             PositionProviderT posOfInterestProvider_,
                             QSharedPointer<SearchPluginI> searchPlugin_,
                             QWidget *parent_ = nullptr)
        : threadpool(threadpool_)
        , cache(cache_)
//        , definitions(definitions_)
        , posOfInterestProvider(posOfInterestProvider_)
        , searchPlugin(searchPlugin_)
        , parent(parent_)
    {}

    QSharedPointer<AsyncTaskProcessorBase> threadpool;
    QSharedPointer<ChunkCache> cache;
//    EntityDefitionsConfig definitions;
    PositionProviderT posOfInterestProvider;
    QSharedPointer<SearchPluginI> searchPlugin;
    QWidget *parent;
};

class SearchChunksWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchChunksWidget(const SearchEntityWidgetInputC& input);
    ~SearchChunksWidget();

    void setVillageLocations(const QList<QSharedPointer<OverlayItem> > &villages);

signals:
    void jumpTo(QVector3D pos);
    void highlightEntities(QVector<QSharedPointer<OverlayItem> >);

private slots:
    void on_pb_search_clicked();

    void chunkLoaded(const QSharedPointer<Chunk> &chunk, int x, int z);

    void on_resultList_jumpTo(const QVector3D &);
    void on_resultList_highlightEntities(QVector<QSharedPointer<OverlayItem> >);

    void displayResults(QSharedPointer<SearchPluginI::ResultListT> results, ChunkID id);

private:
    Ui::SearchChunksWidget *ui;
    SearchEntityWidgetInputC m_input;
    ThreadPoolQtJob m_threadPoolWrapper;
    std::set<ChunkID> m_chunksToSearchList;
    std::set<ChunkID> m_chunksToSearchDoneList;
    QList<QSharedPointer<OverlayItem> > m_villages;
    bool m_searchRunning;
    bool m_requestCancel;

    void checkLocationAndTrySearchChunk(ChunkID id);

    void trySearchChunk(ChunkID id);

    void searchLoadedChunk(const QSharedPointer<Chunk> &chunk);

    bool villageFilter(ChunkID id) const;

    void oneChunkDoneNotify(ChunkID id);
};

#endif // SEARCHENTITYWIDGET_H

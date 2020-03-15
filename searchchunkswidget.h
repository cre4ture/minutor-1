#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include "propertietreecreator.h"
#include "overlayitem.h"
#include "entityevaluator.h"
#include "searchplugininterface.h"
#include "coordinatehashmap.h"
#include "prioritythreadpool.h"
#include "value_initialized.h"
#include "cancellation.h"
#include "safeinvoker.h"
#include "safeprioritythreadpoolwrapper.h"

#include <QWidget>
#include <set>

namespace Ui {
class SearchChunksWidget;
}

class ChunkCache;
class Chunk;
class SearchResultWidget;
class GenericIdentifier;
class PriorityThreadPool;
class SearchTextWidget;

struct SearchEntityWidgetInputC
{
    typedef std::function<QVector3D()> PositionProviderT;

    SearchEntityWidgetInputC(const QSharedPointer<PriorityThreadPool>& threadpool_,
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

    QSharedPointer<PriorityThreadPool> threadpool;
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
    QSharedPointer<Ui::SearchChunksWidget> ui;
    SearchEntityWidgetInputC m_input;
    SafeGuiThreadInvoker m_invoker;
    SimpleSafePriorityThreadPoolWrapper safeThreadPoolI;
    CoordinateHashMap<value_initialized<bool> > m_chunksRequestedToSearchList;
    bool m_searchRunning;
    CancellationTokenWeakPtr currentToken;

    void requestSearchingOfChunk(ChunkID id);

    void searchLoadedChunk(const QSharedPointer<Chunk> &chunk);

    void addOneToProgress(ChunkID id);

    void cancelSearch();
};

#endif // SEARCHENTITYWIDGET_H

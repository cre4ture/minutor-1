#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include "propertietreecreator.h"
#include "overlayitem.h"
#include "entityevaluator.h"
#include "searchplugininterface.h"
#include "coordinatehashmap.h"
#include "prioritythreadpool.h"
#include "value_initialized.h"
#include "executionstatus.h"
#include "safeinvoker.h"
#include "safeprioritythreadpoolwrapper.h"
#include "range.h"

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

    void on_resultList_jumpTo(const QVector3D &);
    void on_resultList_highlightEntities(QVector<QSharedPointer<OverlayItem> >);

    void displayResults(QSharedPointer<SearchPluginI::ResultListT> results, ChunkID id);

private:
    class AsyncSearch;

    QSharedPointer<Ui::SearchChunksWidget> ui;
    SearchEntityWidgetInputC m_input;
    bool m_searchRunning;
    QSharedPointer<AsyncSearch> currentSearch;

    SafeGuiThreadInvoker m_invoker;
    SimpleSafePriorityThreadPoolWrapper safeThreadPoolI;      // must be last member

    class AsyncSearch
    {
    public:
      AsyncSearch(SearchChunksWidget& parent_,
                  const ExecutionStatusToken& currentToken_,
                  const Range<float>& range_y_,
                  QWeakPointer<SearchPluginI> searchPlugin_)
        : parent(parent_)
        , currentToken(currentToken_)
        , range_y(range_y_)
        , searchPlugin(searchPlugin_)
      {}

      void searchLoadedChunk_async(const QSharedPointer<Chunk> &chunk, const ExecutionGuard &guard);

      void searchExistingChunk_async(const QSharedPointer<Chunk> &chunk, const ExecutionGuard &guard);

    private:
      SearchChunksWidget& parent;
      ExecutionStatusToken currentToken;
      const Range<float> range_y;
      QWeakPointer<SearchPluginI> searchPlugin;
    };


    void requestSearchingOfChunk(ChunkID id);

    void addOneToProgress(ChunkID id);

    void cancelSearch();
};

#endif // SEARCHENTITYWIDGET_H

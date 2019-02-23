#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include "propertietreecreator.h"
#include "overlayitem.h"
#include "entityevaluator.h"
#include "searchplugininterface.h"
#include "chunkcachetypes.h"

#include <QWidget>
#include <set>

namespace Ui {
class SearchEntityWidget;
}

class ChunkCache;
class Chunk;
class SearchResultWidget;
class GenericIdentifier;

struct SearchEntityWidgetInputC
{
    typedef std::function<QVector3D()> PositionProviderT;

    SearchEntityWidgetInputC(QSharedPointer<ChunkCache> cache_,
//                             EntityDefitionsConfig definitions_,
                             PositionProviderT posOfInterestProvider_,
                             QSharedPointer<SearchPluginI> searchPlugin_,
                             QWidget *parent_ = nullptr)
        : cache(cache_)
//        , definitions(definitions_)
        , posOfInterestProvider(posOfInterestProvider_)
        , searchPlugin(searchPlugin_)
        , parent(parent_)
    {}

    QSharedPointer<ChunkCache> cache;
//    EntityDefitionsConfig definitions;
    PositionProviderT posOfInterestProvider;
    QSharedPointer<SearchPluginI> searchPlugin;
    QWidget *parent;
};

class SearchEntityWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchEntityWidget(const SearchEntityWidgetInputC& input);
    ~SearchEntityWidget();

signals:
    void jumpTo(QVector3D pos);

private slots:
    void on_pb_search_clicked();

    void chunkLoaded(const QSharedPointer<Chunk> &chunk, int x, int z);

    void on_resultList_jumpTo(const QVector3D &);

private:
    Ui::SearchEntityWidget *ui;
    SearchEntityWidgetInputC m_input;
    std::set<ChunkID> m_searchedBlockCoordinates;

    void trySearchChunk(int x, int z);

    void searchChunk(Chunk &chunk);
};

#endif // SEARCHENTITYWIDGET_H

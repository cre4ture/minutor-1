#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include "propertietreecreator.h"
#include "overlayitem.h"
#include "entityevaluator.h"

#include <QWidget>

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
                             EntityDefitionsConfig definitions_,
                             PositionProviderT posOfInterestProvider_,
                             QWidget *parent_ = nullptr)
        : cache(cache_)
        , definitions(definitions_)
        , posOfInterestProvider(posOfInterestProvider_)
        , parent(parent_)
    {}

    QSharedPointer<ChunkCache> cache;
    EntityDefitionsConfig definitions;
    PositionProviderT posOfInterestProvider;
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

    void chunkLoaded(bool success, int x, int z);

    void on_resultList_jumpTo(const QVector3D &);

private:
    Ui::SearchEntityWidget *ui;
    SearchEntityWidgetInputC m_input;

    void trySearchChunk(int x, int z);

    void searchChunk(Chunk &chunk);

    bool evaluateEntity(EntityEvaluator& entity);
    bool findBuyOrSell(EntityEvaluator& entity, QString searchText, int index);
};

#endif // SEARCHENTITYWIDGET_H

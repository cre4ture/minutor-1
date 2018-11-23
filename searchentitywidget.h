#ifndef SEARCHENTITYWIDGET_H
#define SEARCHENTITYWIDGET_H

#include "propertietreecreator.h"
#include "overlayitem.h"

#include <QWidget>

namespace Ui {
class SearchEntityWidget;
}

class ChunkCache;
class Chunk;
class SearchResultWidget;
class GenericIdentifier;

struct EntityEvaluatorConfig
{
    EntityEvaluatorConfig(SearchResultWidget& resultSink_,
                          QSharedPointer<GenericIdentifier> enchantmentDefintions_,
                          const QString& searchText_,
                          QSharedPointer<OverlayItem> entity_)
        : resultSink(resultSink_)
        , enchantmentDefintions(enchantmentDefintions_)
        , searchText(searchText_)
        , entity(entity_)
    {}

    SearchResultWidget& resultSink;
    QSharedPointer<GenericIdentifier> enchantmentDefintions;
    QString searchText;
    QSharedPointer<OverlayItem> entity;
};

class EntityEvaluator
{
public:
    EntityEvaluator(const EntityEvaluatorConfig& config);

    QList<QString> getOffers() const;

    static const QTreeWidgetItem *getNodeFromPath(const QString path, const QTreeWidgetItem &searchRoot);
    static const QTreeWidgetItem *getNodeFromPath(QStringList::iterator start, QStringList::iterator end, const QTreeWidgetItem &searchRoot);

private:
    EntityEvaluatorConfig m_config;
    QSharedPointer<QTreeWidgetItem> m_rootNode;
    PropertieTreeCreator m_creator;

    void searchProperties();
    void searchTreeNode(const QString prefix, const QTreeWidgetItem& node);

    QString describeReceipe(const QTreeWidgetItem& node) const;
    QString describeReceipeItem(const QTreeWidgetItem& node) const;
};

class SearchEntityWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchEntityWidget(QSharedPointer<ChunkCache> cache,
                                QSharedPointer<GenericIdentifier> enchantmentDefintions,
                                QWidget *parent = nullptr);
    ~SearchEntityWidget();

private slots:
    void on_pb_search_clicked();

    void chunkLoaded(bool success, int x, int z);

private:
    Ui::SearchEntityWidget *ui;
    QSharedPointer<ChunkCache> m_cache;
    QSharedPointer<GenericIdentifier> m_enchantmentDefintions;

    void trySearchChunk(int x, int z);

    void searchChunk(Chunk &chunk);
};

#endif // SEARCHENTITYWIDGET_H
